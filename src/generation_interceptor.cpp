#include "endstone_worldgen/generation_interceptor.h"
#include <algorithm>
#include <optional>
#include <sstream>

namespace endstone_worldgen {
namespace {
bool sameGeometry(const ChunkBuffer &left, const ChunkBuffer &right) noexcept {
    return left.position() == right.position() && left.minY() == right.minY() &&
           left.maxY() == right.maxY();
}

bool hasChanges(const ChunkBuffer &baseline, const ChunkBuffer &desired) {
    if (!sameGeometry(baseline, desired) ||
        baseline.biomeCells() != desired.biomeCells()) return true;
    for (std::int64_t y = baseline.minY(); y <= baseline.maxY(); ++y) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                if (baseline.getRuntimeId(x, static_cast<int>(y), z) !=
                    desired.getRuntimeId(x, static_cast<int>(y), z)) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::optional<ChunkBuffer> mergeDelta(const ChunkBuffer &baseline,
                                      const ChunkBuffer &desired,
                                      ChunkBuffer current,
                                      bool allow_biome_edits) {
    if (!sameGeometry(baseline, desired) || !sameGeometry(baseline, current)) {
        return std::nullopt;
    }

    for (std::int64_t y = baseline.minY(); y <= baseline.maxY(); ++y) {
        const auto block_y = static_cast<int>(y);
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                const auto original_id = baseline.getRuntimeId(x, block_y, z);
                const auto desired_id = desired.getRuntimeId(x, block_y, z);
                if (original_id == desired_id) continue;

                // A live edit landed after the worker snapshot. Abort the
                // entire result before commit instead of overwriting it.
                if (current.getRuntimeId(x, block_y, z) != original_id) {
                    return std::nullopt;
                }
                const auto *descriptor = desired.paletteEntry(desired_id);
                if (!descriptor) return std::nullopt;
                current.setPaletteEntry(desired_id, *descriptor);
                current.setRuntimeId(x, block_y, z, desired_id);
            }
        }
    }

    if (baseline.biomeCells() != desired.biomeCells()) {
        if (!allow_biome_edits) return std::nullopt;
        const auto &original_biomes = baseline.biomeCells();
        const auto &desired_biomes = desired.biomeCells();
        const auto current_biomes = current.biomeCells();

        for (const auto &[key, original_biome] : original_biomes) {
            const auto desired_entry = desired_biomes.find(key);
            if (desired_entry != desired_biomes.end() &&
                desired_entry->second == original_biome) {
                continue;
            }
            const auto current_entry = current_biomes.find(key);
            if (current_entry == current_biomes.end() ||
                current_entry->second != original_biome) {
                return std::nullopt;
            }
            if (desired_entry == desired_biomes.end()) current.eraseBiomeCell(key);
            else current.setBiomeCell(key, desired_entry->second);
        }
        for (const auto &[key, desired_biome] : desired_biomes) {
            if (original_biomes.contains(key)) continue;
            if (current_biomes.contains(key)) return std::nullopt;
            current.setBiomeCell(key, desired_biome);
        }
    }
    return current;
}
} // namespace

GenerationInterceptor::GenerationInterceptor(IVanillaGenerationAdapter &adapter, GenerationScheduler &scheduler,
    std::uint64_t world_seed, InterceptorConfig config)
    : adapter_(adapter), scheduler_(scheduler), world_seed_(world_seed), config_(config) {}
GenerationInterceptor::~GenerationInterceptor(){disable();}

bool GenerationInterceptor::install(){
    std::scoped_lock lock(mutex_);
    if(installed_)return true;
    installed_=adapter_.installChunkRequestInterception(config_.intercept_get_or_load);
    return installed_;
}
void GenerationInterceptor::disable() noexcept{
    std::scoped_lock lock(mutex_);
    if(installed_)adapter_.disableChunkRequestInterception();
    installed_=false;
    waiting_.clear();
    pending_.clear();
    active_keys_.clear();
    stats_.waiting=0;
    stats_.inflight=0;
}
void GenerationInterceptor::addPopulator(std::shared_ptr<IPopulator> populator){
    if(!populator)return;
    std::scoped_lock lock(mutex_);
    populators_.push_back(std::move(populator));
}
void GenerationInterceptor::clearPopulators(){std::scoped_lock lock(mutex_);populators_.clear();}
std::size_t GenerationInterceptor::populatorCount()const{std::scoped_lock lock(mutex_);return populators_.size();}
std::string GenerationInterceptor::keyOf(const NativeChunkRequest&r)const{return r.dimension+":"+std::to_string(r.position.x)+":"+std::to_string(r.position.z);}

void GenerationInterceptor::ingestRequests(){
    auto requests=adapter_.drainInterceptedRequests(config_.max_requests_per_pump);
    stats_.native_requests+=requests.size();
    for(auto &request:requests){
        if(request.kind==NativeChunkRequestKind::GetOrLoadChunk&&!config_.intercept_get_or_load){
            ++stats_.load_requests_ignored;
            continue;
        }
        auto key=keyOf(request);
        if(active_keys_.contains(key)){++stats_.deduplicated;continue;}
        if(waiting_.size()>=config_.max_waiting){++stats_.waiting_overflow_drops;continue;}
        active_keys_.insert(key);
        waiting_.push_back({std::move(request),0});
    }
}
void GenerationInterceptor::dispatchWaiting(){
    if(populators_.empty()){
        stats_.empty_pipeline_requests+=waiting_.size();
        for(const auto &entry:waiting_)active_keys_.erase(keyOf(entry.request));
        waiting_.clear();stats_.waiting=0;return;
    }
    std::size_t captures_now=0;
    const auto candidates=waiting_.size();
    while(captures_now<candidates&&pending_.size()<config_.max_inflight&&
          captures_now<config_.max_captures_per_pump&&!waiting_.empty()){
        ++captures_now;
        auto entry=std::move(waiting_.front());
        waiting_.pop_front();
        auto captured=adapter_.captureChunk(entry.request.dimension,entry.request.position);
        if(!captured){
            ++entry.attempts;++stats_.capture_retries;
            if(entry.attempts>=config_.capture_retry_ticks){active_keys_.erase(keyOf(entry.request));++stats_.capture_failures;}
            else waiting_.push_back(std::move(entry));
            continue;
        }
        GenerationContext context{world_seed_,entry.request.dimension,entry.request.position,GenerationStage::Decoration,0};
        auto baseline=*captured;
        auto future=scheduler_.populatePipeline(std::move(context),std::move(*captured),populators_,config_.priority);
        pending_.push_back({std::move(entry.request),std::move(baseline),std::move(future),std::nullopt,0});++stats_.dispatched;
    }
    stats_.waiting=waiting_.size();stats_.inflight=pending_.size();
}
void GenerationInterceptor::commitReady(){
    std::size_t commit_work_now=0;
    std::size_t examined_now=0;
    const auto candidates=pending_.size();
    for(auto it=pending_.begin();it!=pending_.end()&&
        examined_now<candidates&&commit_work_now<config_.max_commits_per_pump;){
        ++examined_now;
        if(!it->completed){
            if(it->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready){++it;continue;}
            try{
                it->completed=it->future.get();
            }catch(...){
                ++stats_.commit_failures;
                active_keys_.erase(keyOf(it->request));
                it=pending_.erase(it);++commit_work_now;
                continue;
            }
        }

        ++commit_work_now;
        const bool commit_required=hasChanges(it->baseline,it->completed->chunk);
        if(!commit_required){
            active_keys_.erase(keyOf(it->request));
            it=pending_.erase(it);
            continue;
        }

        auto current=adapter_.captureChunk(it->request.dimension,it->request.position);
        if(!current){
            ++it->recapture_attempts;
            ++stats_.capture_retries;
            if(it->recapture_attempts<config_.capture_retry_ticks){
                // Give later ready jobs a turn on the next pump instead of
                // letting one temporarily unavailable chunk monopolize the
                // bounded commit gate for its entire retry window.
                if(it+1!=pending_.end())std::rotate(it,it+1,pending_.end());
                else ++it;
                continue;
            }
            ++stats_.capture_failures;
            ++stats_.commit_failures;
            active_keys_.erase(keyOf(it->request));
            it=pending_.erase(it);
            continue;
        }

        bool ok=false;
        try{
            if(auto merged=mergeDelta(
                   it->baseline,it->completed->chunk,std::move(*current),
                   adapter_.capabilities().biome_edits)){
                ok=adapter_.commitChunk(it->request.dimension,*merged);
                if(ok) ok=adapter_.flushThreadBatch(it->request.dimension);
            }
        }catch(...){ok=false;}
        if(ok)++stats_.committed;else ++stats_.commit_failures;
        active_keys_.erase(keyOf(it->request));it=pending_.erase(it);
    }
    stats_.inflight=pending_.size();
}
InterceptorStats GenerationInterceptor::pump(){
    std::scoped_lock lock(mutex_);
    if(!installed_)return stats_;
    ingestRequests();commitReady();dispatchWaiting();return stats_;
}
InterceptorStats GenerationInterceptor::stats()const{std::scoped_lock lock(mutex_);return stats_;}
}
