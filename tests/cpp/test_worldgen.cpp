#include "endstone_worldgen/generation_interceptor.h"
#include "endstone_worldgen/generation_scheduler.h"
#include "endstone_worldgen/bds_26_30_adapter.h"
#include <cassert>
#include <chrono>
#include <deque>
#include <iostream>
#include <thread>
using namespace endstone_worldgen;

class OrePop final : public IPopulator {
public:
    std::string_view identifier() const noexcept override { return "test:ore"; }
    int radius() const noexcept override { return 1; }
    void populate(const GenerationContext &c, ChunkBuffer &b) override {
        b.setRuntimeId(int(c.stage_seed % 16), 0, int((c.stage_seed >> 8) % 16), 42);
    }
};

class MockNativeAdapter final : public IVanillaGenerationAdapter {
public:
    std::string bedrockBuild() const override { return "1.26.33"; }
    bool verifySymbols() noexcept override { return true; }
    NativeCapabilities capabilities() const noexcept override {
        NativeCapabilities c; c.capture_chunk = c.commit_chunk = c.chunk_request_interception = true;
        c.detached_worker_dispatch = c.primary_thread_commit_gate = true; return c;
    }
    NativeDiagnostics diagnostics() const override {
        NativeDiagnostics d; d.adapter = "mock"; d.exact_build_match = true;
        d.interception_installed = installed; d.intercepted_requests = intercepted; return d;
    }
    std::optional<ChunkBuffer> captureChunk(const std::string &, ChunkPos pos) override {
        ChunkBuffer buffer(pos, 0, 15, 1);
        buffer.setPaletteEntry(1, {"minecraft:stone", {}});
        buffer.setPaletteEntry(42, {"minecraft:diamond_ore", {}});
        return buffer;
    }
    bool commitChunk(const std::string &, const ChunkBuffer &buffer) override {
        committed = buffer; ++commit_count; return true;
    }
    bool flushThreadBatch(const std::string &) override { ++flush_count; return true; }
    bool installChunkRequestInterception(bool include_get_or_load = false) override { installed = true; intercept_loads = include_get_or_load; return true; }
    void disableChunkRequestInterception() noexcept override { installed = false; }
    std::vector<NativeChunkRequest> drainInterceptedRequests(std::size_t maximum) override {
        std::vector<NativeChunkRequest> out;
        while (!requests.empty() && out.size() < maximum) {
            out.push_back(requests.front()); requests.pop_front();
        }
        return out;
    }
    void push(ChunkPos pos) {
        requests.push_back({"overworld", pos, NativeChunkRequestKind::CreateNewChunk, false, ++intercepted});
    }

    bool installed{};
    bool intercept_loads{};
    std::uint64_t intercepted{};
    int commit_count{};
    int flush_count{};
    std::optional<ChunkBuffer> committed;
    std::deque<NativeChunkRequest> requests;
};

int main() {
    assert(isSupportedBds2630Build("1.26.32"));
    assert(isSupportedBds2630Build("1.26.33"));
    assert(!isSupportedBds2630Build("1.26.20"));

    GenerationScheduler scheduler(4);
    GenerationContext context{1234, "overworld", {2, 3}, GenerationStage::BaseTerrain, 0};
    auto generator = std::make_shared<FlatGenerator>(64, 1, 2);
    auto first = scheduler.generate(context, generator, JobPriority::PlayerRequested).get();
    auto second = scheduler.generate(context, generator, JobPriority::Background).get();
    assert(first.fingerprint == second.fingerprint);
    assert(first.chunk.getRuntimeId(0, 64, 0) == 2);
    first.chunk.setPaletteEntry(2, {"minecraft:grass_block", {}});
    assert(first.chunk.paletteEntry(2) != nullptr);
    assert(first.chunk.paletteSize() == 1);
    context.stage = GenerationStage::Features;
    auto populated = scheduler.populate(context, std::move(first.chunk), std::make_shared<OrePop>()).get();
    assert(populated.fingerprint != second.fingerprint);

    MockNativeAdapter adapter;
    GenerationInterceptor interceptor(adapter, scheduler, 9876);
    interceptor.addPopulator(std::make_shared<OrePop>());
    assert(interceptor.install());
    adapter.push({7, 9});
    adapter.push({7, 9}); // deduplication must prevent a second concurrent job.
    for (int i = 0; i < 200 && adapter.commit_count == 0; ++i) {
        interceptor.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto stats = interceptor.stats();
    assert(stats.native_requests == 2);
    assert(stats.deduplicated == 1);
    assert(stats.dispatched == 1);
    assert(stats.committed == 1);
    assert(adapter.commit_count == 1);
    assert(adapter.flush_count == 1);
    assert(adapter.committed.has_value());
    interceptor.disable();
    assert(!adapter.installed);

    std::cout << "worldgen tests passed\n";
}
