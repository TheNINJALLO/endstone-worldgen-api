#include "endstone_worldgen/generation_interceptor.h"
#include "endstone_worldgen/generation_scheduler.h"
#include "endstone_worldgen/bds_26_30_adapter.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <deque>
#include <iostream>
#include <limits>
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

class CountingPop final : public IPopulator {
public:
    explicit CountingPop(std::atomic<int> &completed) : completed_(completed) {}
    std::string_view identifier() const noexcept override { return "test:counting"; }
    int radius() const noexcept override { return 0; }
    void populate(const GenerationContext &, ChunkBuffer &) override {
        completed_.fetch_add(1, std::memory_order_release);
    }
private:
    std::atomic<int> &completed_;
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
        ++capture_count;
        if (failed_capture && *failed_capture == pos) return std::nullopt;
        ChunkBuffer buffer(pos, 0, 15, 1);
        buffer.setPaletteEntry(1, {"minecraft:stone", {}});
        buffer.setPaletteEntry(42, {"minecraft:diamond_ore", {}});
        return buffer;
    }
    bool commitChunk(const std::string &, const ChunkBuffer &buffer) override {
        committed = buffer; ++commit_count; return true;
    }
    bool flushThreadBatch(const std::string &) override { ++flush_count; return flush_succeeds; }
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
    bool flush_succeeds{true};
    std::uint64_t intercepted{};
    int commit_count{};
    int flush_count{};
    int capture_count{};
    std::optional<ChunkPos> failed_capture;
    std::optional<ChunkBuffer> committed;
    std::deque<NativeChunkRequest> requests;
};

int main() {
    assert(isSupportedBds2630Build("1.26.32"));
    assert(isSupportedBds2630Build("1.26.33"));
    assert(isSupportedBds2630Build("26.32"));
    assert(isSupportedBds2630Build("26.33"));
    assert(!isSupportedBds2630Build(""));
    assert(!isSupportedBds2630Build("1.26.30"));
    assert(!isSupportedBds2630Build("1.26.20"));
    assert(!isSupportedBds2630Build("1.26.34"));
    assert(!isSupportedBds2630Build("server-26.33"));
    assert(isExpectedBds2630Build("26.33", "1.26.33"));
    assert(!isExpectedBds2630Build("26.32", "1.26.33"));
    assert(isExpectedEndstoneVersion("0.11.6", "0.11.6"));
    assert(isExpectedEndstoneVersion("v0.11.5", "0.11.5"));
    assert(!isExpectedEndstoneVersion("0.11.6-dev", "0.11.6"));

    assert(deterministicStringHash("overworld") == 0xd4447a2733c45e2dULL);
    assert(deterministicStageSeed(1234, "overworld", {2, 3}, GenerationStage::BaseTerrain) ==
           0xbc6a9b6f10aa3ef4ULL);

    bool invalid_range_threw = false;
    try { ChunkBuffer invalid({0, 0}, 10, 9); }
    catch (const std::invalid_argument &) { invalid_range_threw = true; }
    assert(invalid_range_threw);

    ChunkBuffer bounded({0, 0}, 0, 15, 0);
    bool invalid_biome_threw = false;
    try { bounded.setBiome(16, 0, 0, 1); }
    catch (const std::out_of_range &) { invalid_biome_threw = true; }
    assert(invalid_biome_threw);

    ChunkBuffer palette_validation({0, 0}, 0, 1, 1);
    assert(!palette_validation.hasCompletePalette());
    palette_validation.setPaletteEntry(1, {"minecraft:stone", {}});
    assert(palette_validation.hasCompletePalette());
    palette_validation.setRuntimeId(0, 0, 0, 2);
    assert(!palette_validation.hasCompletePalette());
    palette_validation.setPaletteEntry(2, {"minecraft:grass_block", {}});
    assert(palette_validation.hasCompletePalette());

    ChunkBuffer fingerprint_a({4, -7}, 0, 3, 9);
    fingerprint_a.setBiome(0, 0, 0, 2);
    fingerprint_a.setBiome(4, 0, 0, 3);
    assert(fingerprint_a.biomeCellCount() == 2);
    BlockDescriptor descriptor_a{"minecraft:test_block", {}};
    descriptor_a.states.emplace("label", std::string("stable"));
    descriptor_a.states.emplace("age", std::int32_t{2});
    descriptor_a.states.emplace("powered", true);
    fingerprint_a.setPaletteEntry(9, descriptor_a);

    ChunkBuffer fingerprint_b({4, -7}, 0, 3, 9);
    fingerprint_b.setBiome(4, 0, 0, 3);
    fingerprint_b.setBiome(0, 0, 0, 2);
    BlockDescriptor descriptor_b{"minecraft:test_block", {}};
    descriptor_b.states.emplace("powered", true);
    descriptor_b.states.emplace("age", std::int32_t{2});
    descriptor_b.states.emplace("label", std::string("stable"));
    fingerprint_b.setPaletteEntry(9, descriptor_b);
    assert(fingerprint_a.fingerprint() == fingerprint_b.fingerprint());

    ChunkBuffer different_position({5, -7}, 0, 3, 9);
    different_position.setBiome(0, 0, 0, 2);
    different_position.setBiome(4, 0, 0, 3);
    different_position.setPaletteEntry(9, descriptor_a);
    assert(fingerprint_a.fingerprint() != different_position.fingerprint());
    auto different_biome = fingerprint_a;
    different_biome.setBiome(0, 0, 0, 4);
    assert(fingerprint_a.fingerprint() != different_biome.fingerprint());
    auto different_palette = fingerprint_a;
    different_palette.setPaletteEntry(9, {"minecraft:other_block", {}});
    assert(fingerprint_a.fingerprint() != different_palette.fingerprint());

    ChunkBuffer parity_fixture({2, -3}, 0, 0, 7);
    assert(parity_fixture.fingerprint() == 0xc8b312f0a708b926ULL);
    parity_fixture.setBiome(0, 0, 0, 42);
    BlockDescriptor parity_descriptor{"minecraft:stone", {}};
    parity_descriptor.states.emplace("age", std::int32_t{3});
    parity_descriptor.states.emplace("label", std::string("x"));
    parity_descriptor.states.emplace("powered", true);
    parity_fixture.setPaletteEntry(7, std::move(parity_descriptor));
    assert(parity_fixture.fingerprint() == 0xb9f0ca7d18902b33ULL);

    ChunkBuffer maximum_y({0, 0}, std::numeric_limits<int>::max(),
                          std::numeric_limits<int>::max(), 0);
    maximum_y.fill(0, std::numeric_limits<int>::max(), 0, 15,
                   std::numeric_limits<int>::max(), 15, 17);
    assert(maximum_y.getRuntimeId(15, std::numeric_limits<int>::max(), 15) == 17);

    NeighborhoodLockManager neighborhood_locks;
    bool negative_radius_threw = false;
    try { (void)neighborhood_locks.acquire({0, 0}, -1); }
    catch (const std::invalid_argument &) { negative_radius_threw = true; }
    assert(negative_radius_threw);
    bool overflowing_radius_threw = false;
    try { (void)neighborhood_locks.acquire({std::numeric_limits<int>::max(), 0}, 1); }
    catch (const std::out_of_range &) { overflowing_radius_threw = true; }
    assert(overflowing_radius_threw);

    FlatGenerator above_range(100, 5, 6);
    above_range.generate({}, bounded);
    assert(bounded.getRuntimeId(0, 15, 0) == 5);
    HashTerrainGenerator out_of_range_terrain(100, 7, 8);
    out_of_range_terrain.generate({}, bounded); // Out-of-range surfaces are clipped, not thrown.

    GenerationScheduler scheduler(4);
    bool null_generator_threw = false;
    try { (void)scheduler.generate({}, {}); }
    catch (const std::invalid_argument &) { null_generator_threw = true; }
    assert(null_generator_threw);
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

    // Disable must clear active request keys so the same chunk can be handled
    // after a plugin reload/reinstall.
    assert(interceptor.install());
    adapter.push({7, 9});
    for (int i = 0; i < 200 && adapter.commit_count == 1; ++i) {
        interceptor.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(adapter.commit_count == 2);
    interceptor.disable();

    // Capturing scans a live chunk through the Endstone API and must remain
    // strictly bounded on the primary thread even when the queue is full.
    MockNativeAdapter capture_adapter;
    GenerationInterceptor capture_limited(capture_adapter, scheduler, 1234);
    capture_limited.addPopulator(std::make_shared<OrePop>());
    assert(capture_limited.install());
    capture_adapter.push({100, 0});
    capture_adapter.push({200, 0});
    capture_adapter.push({300, 0});
    capture_adapter.failed_capture = ChunkPos{100, 0};
    auto capture_stats = capture_limited.pump();
    assert(capture_adapter.capture_count == 1);
    assert(capture_stats.dispatched == 0);
    assert(capture_stats.waiting == 3);
    capture_stats = capture_limited.pump();
    assert(capture_adapter.capture_count == 2);
    assert(capture_stats.dispatched == 1);
    assert(capture_stats.waiting == 2);
    capture_limited.disable();

    // Bursts are bounded even when ingestion is faster than the configured
    // primary-thread capture budget.
    MockNativeAdapter overflow_adapter;
    InterceptorConfig overflow_config;
    overflow_config.max_requests_per_pump = 3;
    overflow_config.max_captures_per_pump = 0;
    overflow_config.max_waiting = 2;
    GenerationInterceptor overflow_limited(overflow_adapter, scheduler, 1234, overflow_config);
    overflow_limited.addPopulator(std::make_shared<OrePop>());
    assert(overflow_limited.install());
    overflow_adapter.push({700, 0});
    overflow_adapter.push({701, 0});
    overflow_adapter.push({702, 0});
    const auto overflow_stats = overflow_limited.pump();
    assert(overflow_stats.waiting == 2);
    assert(overflow_stats.waiting_overflow_drops == 1);
    overflow_limited.disable();

    // Completed worker results can accumulate, but only the configured number
    // may be written back to the live world in one pump.
    MockNativeAdapter commit_adapter;
    InterceptorConfig commit_config;
    commit_config.max_captures_per_pump = 3;
    commit_config.max_commits_per_pump = 1;
    commit_config.max_inflight = 3;
    GenerationInterceptor commit_limited(commit_adapter, scheduler, 1234, commit_config);
    std::atomic<int> completed_jobs{};
    commit_limited.addPopulator(std::make_shared<CountingPop>(completed_jobs));
    assert(commit_limited.install());
    commit_adapter.push({400, 0});
    commit_adapter.push({500, 0});
    commit_adapter.push({600, 0});
    commit_limited.pump();
    assert(commit_adapter.capture_count == 3);
    for (int i = 0; i < 200 && completed_jobs.load(std::memory_order_acquire) < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(completed_jobs.load(std::memory_order_acquire) == 3);
    InterceptorStats commit_stats;
    for (int i = 0; i < 200 && commit_adapter.commit_count < 2; ++i) {
        const auto before_pump = commit_adapter.commit_count;
        commit_stats = commit_limited.pump();
        assert(commit_adapter.commit_count - before_pump <= 1);
        if (commit_adapter.commit_count < 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(commit_adapter.commit_count == 2);
    commit_adapter.flush_succeeds = false;
    for (int i = 0; i < 200 && commit_adapter.commit_count < 3; ++i) {
        const auto before_pump = commit_adapter.commit_count;
        commit_stats = commit_limited.pump();
        assert(commit_adapter.commit_count - before_pump <= 1);
        if (commit_adapter.commit_count < 3)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(commit_adapter.commit_count == 3);
    assert(commit_stats.committed == 2);
    assert(commit_stats.commit_failures == 1);
    assert(commit_stats.inflight == 0);
    commit_limited.disable();

    std::cout << "worldgen tests passed\n";
}
