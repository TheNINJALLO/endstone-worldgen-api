#include "endstone_worldgen/bds_26_30_adapter.h"
#include <endstone/endstone.hpp>
#include "bedrock/world/level/dimension/dimension.h"
#include "bedrock/world/level/chunk/chunk_source.h"
#include "endstone/core/level/dimension.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace endstone_worldgen {
namespace {
std::pair<int, int> heightRange(endstone::Dimension::Type type) {
    switch (type) {
    case endstone::Dimension::Type::Nether: return {0, 127};
    case endstone::Dimension::Type::TheEnd: return {0, 255};
    default: return {-64, 319};
    }
}

struct HookQueue {
    std::mutex mutex;
    std::deque<NativeChunkRequest> requests;
    std::atomic<std::uint64_t> next_sequence{1};
    std::atomic<std::uint64_t> intercepted{0};
    std::atomic<std::uint64_t> dropped{0};
    std::size_t maximum{4096};
    bool include_get_or_load{};
};

struct SourceBinding {
    std::weak_ptr<HookQueue> queue;
    std::string dimension;
    void **original_vtable{};
    std::unique_ptr<void *[]> shadow_storage;
    void **shadow_vtable{};
    using CreateFn = std::shared_ptr<LevelChunk> (*)(ChunkSource *, const ::ChunkPos &, ChunkSource::LoadMode, bool);
    using LoadFn = std::shared_ptr<LevelChunk> (*)(ChunkSource *, const ::ChunkPos &, ChunkSource::LoadMode, bool);
    CreateFn original_create{};
    LoadFn original_load{};
    bool load_hooked{};
};

std::mutex g_hook_mutex;
std::unordered_map<ChunkSource *, SourceBinding> g_source_bindings;

void enqueueRequest(ChunkSource *source, NativeChunkRequestKind kind, const ::ChunkPos &position, bool read_only) {
    std::shared_ptr<HookQueue> queue;
    std::string dimension;
    {
        std::scoped_lock lock(g_hook_mutex);
        auto found = g_source_bindings.find(source);
        if (found == g_source_bindings.end()) return;
        queue = found->second.queue.lock();
        dimension = found->second.dimension;
    }
    if (!queue) return;
    if (kind == NativeChunkRequestKind::GetOrLoadChunk && !queue->include_get_or_load) return;

    NativeChunkRequest request;
    request.dimension = std::move(dimension);
    request.position = {position.x, position.z};
    request.kind = kind;
    request.read_only = read_only;
    request.sequence = queue->next_sequence.fetch_add(1, std::memory_order_relaxed);

    std::scoped_lock lock(queue->mutex);
    if (queue->requests.size() >= queue->maximum) {
        queue->dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    queue->requests.push_back(std::move(request));
    queue->intercepted.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<LevelChunk> createNewChunkDetour(ChunkSource *self, const ::ChunkPos &position,
                                                  ChunkSource::LoadMode mode, bool read_only) {
    SourceBinding::CreateFn original{};
    {
        std::scoped_lock lock(g_hook_mutex);
        auto found = g_source_bindings.find(self);
        if (found != g_source_bindings.end()) original = found->second.original_create;
    }
    if (!original) return {};
    auto result = original(self, position, mode, read_only);
    if (result) enqueueRequest(self, NativeChunkRequestKind::CreateNewChunk, position, read_only);
    return result;
}

std::shared_ptr<LevelChunk> getOrLoadChunkDetour(ChunkSource *self, const ::ChunkPos &position,
                                                 ChunkSource::LoadMode mode, bool read_only) {
    SourceBinding::LoadFn original{};
    {
        std::scoped_lock lock(g_hook_mutex);
        auto found = g_source_bindings.find(self);
        if (found != g_source_bindings.end()) original = found->second.original_load;
    }
    if (!original) return {};
    auto result = original(self, position, mode, read_only);
    if (result) enqueueRequest(self, NativeChunkRequestKind::GetOrLoadChunk, position, read_only);
    return result;
}

// ChunkSource has one destructor vtable slot under MSVC and the Itanium ABI has
// complete + deleting destructor slots. These ordinals are pinned to the exact
// Endstone v0.11.5/v0.11.6 class declaration and are runtime-gated by BDS build.
#ifdef _WIN32
inline constexpr int kVtablePrefix = 1; // MSVC complete-object locator
inline constexpr int kCreateNewChunkOrdinal = 7;
inline constexpr int kGetOrLoadChunkOrdinal = 8;
inline constexpr int kChunkSourceVirtualCount = 39;
#else
inline constexpr int kVtablePrefix = 2; // Itanium offset-to-top + typeinfo
inline constexpr int kCreateNewChunkOrdinal = 8;
inline constexpr int kGetOrLoadChunkOrdinal = 9;
inline constexpr int kChunkSourceVirtualCount = 40;
#endif

bool installShadowVtable(ChunkSource *source, const std::shared_ptr<HookQueue> &queue,
                         std::string dimension, bool include_get_or_load, SourceBinding &binding) {
    if (!source) return false;
    auto **original = *reinterpret_cast<void ***>(source);
    if (!original) return false;

    auto storage = std::make_unique<void *[]>(kVtablePrefix + kChunkSourceVirtualCount);
    auto **shadow = storage.get() + kVtablePrefix;
    for (int i = -kVtablePrefix; i < kChunkSourceVirtualCount; ++i) shadow[i] = original[i];

    auto create = reinterpret_cast<SourceBinding::CreateFn>(original[kCreateNewChunkOrdinal]);
    auto load = reinterpret_cast<SourceBinding::LoadFn>(original[kGetOrLoadChunkOrdinal]);
    if (!create || !load) return false;
    shadow[kCreateNewChunkOrdinal] = reinterpret_cast<void *>(&createNewChunkDetour);
    if (include_get_or_load)
        shadow[kGetOrLoadChunkOrdinal] = reinterpret_cast<void *>(&getOrLoadChunkDetour);

    binding.queue = queue;
    binding.dimension = std::move(dimension);
    binding.original_vtable = original;
    binding.shadow_storage = std::move(storage);
    binding.shadow_vtable = shadow;
    binding.original_create = create;
    binding.original_load = load;
    binding.load_hooked = include_get_or_load;
    *reinterpret_cast<void ***>(source) = shadow;
    return true;
}

void restoreShadowVtable(ChunkSource *source, SourceBinding &binding) noexcept {
    if (!source || !binding.original_vtable) return;
    auto ***object_vptr = reinterpret_cast<void ***>(source);
    if (*object_vptr == binding.shadow_vtable) *object_vptr = binding.original_vtable;
}

class Bds2630WorldGenAdapter final : public IVanillaGenerationAdapter {
public:
    explicit Bds2630WorldGenAdapter(endstone::Server &server)
        : server_(server), queue_(std::make_shared<HookQueue>()) {}

    ~Bds2630WorldGenAdapter() override { disableChunkRequestInterception(); }

    std::string bedrockBuild() const override { return server_.getMinecraftVersion(); }

    bool verifySymbols() noexcept override {
        try {
            return server_.isPrimaryThread() && isSupportedBds2630Build(server_.getMinecraftVersion()) &&
                   sizeof(void *) == 8;
        } catch (...) {
            return false;
        }
    }

    NativeCapabilities capabilities() const noexcept override {
        NativeCapabilities c;
        c.capture_chunk = true;
        c.commit_chunk = true;
        c.chunk_source_probe = true;
        c.bds_task_launch_visible = true;
        c.chunk_request_interception = true;
        c.exact_vtable_hooks = true;
        c.detached_worker_dispatch = true;
        c.primary_thread_commit_gate = true;
        // Registered custom population/post-processing is actually dispatched
        // to detached workers. Mojang's opaque base-terrain generator is not
        // called from foreign threads by this adapter.
        c.vanilla_post_processing = true;
        c.vanilla_hybrid = false;
        c.vanilla_native_stages = false;
        return c;
    }

    NativeDiagnostics diagnostics() const override {
        auto out = diagnostics_;
        out.adapter = "bds-26.30-exact-hooked";
        out.runtime_build = server_.getMinecraftVersion();
        out.exact_build_match = isSupportedBds2630Build(out.runtime_build);
        out.primary_thread = server_.isPrimaryThread();
        out.intercepted_requests = queue_->intercepted.load(std::memory_order_relaxed);
        out.dropped_requests = queue_->dropped.load(std::memory_order_relaxed);
        {
            std::scoped_lock lock(g_hook_mutex);
            std::size_t count{};
            for (const auto &[source, binding] : g_source_bindings) {
                if (binding.queue.lock() == queue_) ++count;
            }
            out.hooked_chunk_sources = count;
            out.interception_installed = installed_ && count > 0;
        }
        return out;
    }

    std::optional<ChunkBuffer> captureChunk(const std::string &dimension_name, ChunkPos pos) override {
        if (!verifySymbols()) return std::nullopt;
        auto *level = server_.getLevel();
        auto *dimension = level ? level->getDimension(dimension_name) : nullptr;
        auto *exact = dynamic_cast<endstone::core::EndstoneDimension *>(dimension);
        if (!dimension || !exact) return std::nullopt;

        auto [min_y, max_y] = heightRange(dimension->getType());
        ChunkBuffer buffer(pos, min_y, max_y, 0);
        auto &source = exact->getHandle().getBlockSourceFromMainChunkSource();
        diagnostics_.chunk_source_available = true;
        diagnostics_.chunk_source_can_launch_tasks = source.getChunkSource().canLaunchTasks();

        for (int y = min_y; y <= max_y; ++y) {
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    const int world_x = pos.x * 16 + x;
                    const int world_z = pos.z * 16 + z;
                    auto block = dimension->getBlockAt(world_x, y, world_z);
                    if (!block) continue;
                    auto data = block->getData();
                    if (!data) continue;
                    const auto id = data->getRuntimeId();
                    buffer.setRuntimeId(x, y, z, id);
                    if (!buffer.paletteEntry(id)) {
                        BlockDescriptor descriptor;
                        descriptor.type = data->getType();
                        for (const auto &[key, value] : data->getBlockStates()) {
                            std::visit([&](const auto &v) { descriptor.states[key] = v; }, value);
                        }
                        buffer.setPaletteEntry(id, std::move(descriptor));
                    }
                }
            }
        }
        return buffer;
    }

    bool commitChunk(const std::string &dimension_name, const ChunkBuffer &buffer) override {
        if (!verifySymbols()) return false;
        auto *level = server_.getLevel();
        auto *dimension = level ? level->getDimension(dimension_name) : nullptr;
        if (!dimension) return false;

        std::unordered_map<std::uint32_t, std::unique_ptr<endstone::BlockData>> native_palette;
        for (int y = buffer.minY(); y <= buffer.maxY(); ++y) {
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    const auto id = buffer.getRuntimeId(x, y, z);
                    auto block = dimension->getBlockAt(buffer.position().x * 16 + x, y,
                                                       buffer.position().z * 16 + z);
                    if (!block) return false;
                    auto current = block->getData();
                    if (current && current->getRuntimeId() == id) continue;

                    auto it = native_palette.find(id);
                    if (it == native_palette.end()) {
                        const auto *descriptor = buffer.paletteEntry(id);
                        if (!descriptor) return false;
                        endstone::BlockStates states;
                        for (const auto &[key, value] : descriptor->states) {
                            std::visit([&](const auto &v) { states[key] = v; }, value);
                        }
                        auto data = server_.createBlockData(descriptor->type, std::move(states));
                        if (!data) return false;
                        it = native_palette.emplace(id, std::move(data)).first;
                    }
                    block->setData(*it->second, false);
                }
            }
        }
        return true;
    }

    bool flushThreadBatch(const std::string &dimension_name) override {
        if (!verifySymbols()) return false;
        auto *level = server_.getLevel();
        auto *dimension = level ? level->getDimension(dimension_name) : nullptr;
        auto *exact = dynamic_cast<endstone::core::EndstoneDimension *>(dimension);
        if (!exact) return false;
        auto &source = exact->getHandle().getBlockSourceFromMainChunkSource();
        source.getChunkSource().flushThreadBatch();
        diagnostics_.chunk_source_available = true;
        diagnostics_.chunk_source_can_launch_tasks = source.getChunkSource().canLaunchTasks();
        return true;
    }

    bool installChunkRequestInterception(bool include_get_or_load = false) override {
        if (!verifySymbols()) return false;
        queue_->include_get_or_load = include_get_or_load;
        auto *level = server_.getLevel();
        if (!level) return false;

        std::size_t bound{};
        try {
            for (auto *dimension : level->getDimensions()) {
                auto *exact = dynamic_cast<endstone::core::EndstoneDimension *>(dimension);
                if (!exact) continue;
                auto *source = &exact->getHandle().getBlockSourceFromMainChunkSource().getChunkSource();
                std::scoped_lock lock(g_hook_mutex);
                if (g_source_bindings.contains(source)) {
                    ++bound;
                    continue;
                }
                SourceBinding binding;
                if (!installShadowVtable(source, queue_, dimension->getName(), include_get_or_load, binding)) {
                    queue_->dropped.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                g_source_bindings.emplace(source, std::move(binding));
                ++bound;
            }
        } catch (...) {
            disableChunkRequestInterception();
            return false;
        }
        installed_ = bound > 0;
        diagnostics_.interception_installed = installed_;
        diagnostics_.hooked_chunk_sources = bound;
        return installed_;
    }

    void disableChunkRequestInterception() noexcept override {
        std::scoped_lock lock(g_hook_mutex);
        for (auto it = g_source_bindings.begin(); it != g_source_bindings.end();) {
            if (it->second.queue.lock() == queue_) {
                restoreShadowVtable(it->first, it->second);
                it = g_source_bindings.erase(it);
            } else {
                ++it;
            }
        }
        installed_ = false;
        diagnostics_.interception_installed = false;
        diagnostics_.hooked_chunk_sources = 0;
    }

    std::vector<NativeChunkRequest> drainInterceptedRequests(std::size_t maximum) override {
        std::vector<NativeChunkRequest> out;
        if (!server_.isPrimaryThread() || maximum == 0) return out;
        std::scoped_lock lock(queue_->mutex);
        const auto count = std::min(maximum, queue_->requests.size());
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.push_back(std::move(queue_->requests.front()));
            queue_->requests.pop_front();
        }
        return out;
    }

private:
    endstone::Server &server_;
    std::shared_ptr<HookQueue> queue_;
    mutable NativeDiagnostics diagnostics_{};
    bool installed_{};
};
} // namespace

std::unique_ptr<IVanillaGenerationAdapter> makeBds2630WorldGenAdapter(endstone::Server &server) {
    auto adapter = std::make_unique<Bds2630WorldGenAdapter>(server);
    if (!adapter->verifySymbols()) return {};
    return adapter;
}
} // namespace endstone_worldgen
