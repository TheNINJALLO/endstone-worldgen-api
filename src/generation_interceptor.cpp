#include "endstone_worldgen/generation_interceptor.h"
#include <algorithm>
#include <sstream>

namespace endstone_worldgen {
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
    for(auto it=waiting_.begin();it!=waiting_.end()&&pending_.size()<config_.max_inflight;){
        auto captured=adapter_.captureChunk(it->request.dimension,it->request.position);
        if(!captured){
            ++it->attempts;++stats_.capture_retries;
            if(it->attempts>=config_.capture_retry_ticks){active_keys_.erase(keyOf(it->request));it=waiting_.erase(it);++stats_.capture_failures;}else ++it;
            continue;
        }
        GenerationContext context{world_seed_,it->request.dimension,it->request.position,GenerationStage::Decoration,0};
        auto future=scheduler_.populatePipeline(std::move(context),std::move(*captured),populators_,config_.priority);
        pending_.push_back({it->request,std::move(future)});it=waiting_.erase(it);++stats_.dispatched;
    }
    stats_.waiting=waiting_.size();stats_.inflight=pending_.size();
}
void GenerationInterceptor::commitReady(){
    std::size_t committed_now=0;
    for(auto it=pending_.begin();it!=pending_.end()&&committed_now<config_.max_commits_per_pump;){
        if(it->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready){++it;continue;}
        bool ok=false;
        try{
            auto result=it->future.get();
            ok=adapter_.commitChunk(it->request.dimension,result.chunk);
            if(ok) adapter_.flushThreadBatch(it->request.dimension);
        }catch(...){ok=false;}
        if(ok)++stats_.committed;else ++stats_.commit_failures;
        active_keys_.erase(keyOf(it->request));it=pending_.erase(it);++committed_now;
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
