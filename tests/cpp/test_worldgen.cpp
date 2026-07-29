#include "endstone_worldgen/generation_interceptor.h"
#include "endstone_worldgen/generation_scheduler.h"
#include "endstone_worldgen/live_generation.h"
#include "endstone_worldgen/bds_26_30_adapter.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <deque>
#include <iostream>
#include <limits>
#include <thread>
#include <unordered_map>
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
    void populate(const GenerationContext &, ChunkBuffer &buffer) override {
        buffer.setRuntimeId(0, 0, 0, 42);
        completed_.fetch_add(1, std::memory_order_release);
    }
private:
    std::atomic<int> &completed_;
};

class NoOpPop final : public IPopulator {
public:
    std::string_view identifier() const noexcept override { return "test:no_op"; }
    int radius() const noexcept override { return 0; }
    void populate(const GenerationContext &, ChunkBuffer &) override {}
};

class FixedPop final : public IPopulator {
public:
    std::string_view identifier() const noexcept override { return "test:fixed"; }
    int radius() const noexcept override { return 0; }
    void populate(const GenerationContext &, ChunkBuffer &buffer) override {
        buffer.setPaletteEntry(77, {"minecraft:new_test_block", {}});
        buffer.setRuntimeId(1, 0, 1, 77);
    }
};

class MissingPalettePop final : public IPopulator {
public:
    std::string_view identifier() const noexcept override { return "test:missing_palette"; }
    int radius() const noexcept override { return 0; }
    void populate(const GenerationContext &, ChunkBuffer &buffer) override {
        buffer.setRuntimeId(1, 0, 1, 78);
    }
};

class BiomePop final : public IPopulator {
public:
    std::string_view identifier() const noexcept override { return "test:biome"; }
    int radius() const noexcept override { return 0; }
    void populate(const GenerationContext &, ChunkBuffer &buffer) override {
        buffer.setBiome(0, 0, 0, 7);
    }
};

class MockNativeAdapter final : public IVanillaGenerationAdapter {
public:
    std::string bedrockBuild() const override { return "1.26.33"; }
    bool verifySymbols() noexcept override { return true; }
    NativeCapabilities capabilities() const noexcept override {
        NativeCapabilities c; c.capture_chunk = c.commit_chunk = c.chunk_request_interception = true;
        c.detached_worker_dispatch = c.primary_thread_commit_gate = true;
        c.biome_edits = biome_edits_supported; return c;
    }
    NativeDiagnostics diagnostics() const override {
        NativeDiagnostics d; d.adapter = "mock"; d.exact_build_match = true;
        d.primary_thread = primary_thread; d.interception_installed = installed;
        d.intercepted_requests = intercepted; return d;
    }
    std::optional<ResolvedBlockDescriptor> resolveBlock(
        const std::string &type, const DescriptorStates &states = {}) override {
        if (failed_descriptor && *failed_descriptor == type) return std::nullopt;
        static const std::unordered_map<std::string, std::uint32_t> ids{
            {"minecraft:stone", 1},
            {"minecraft:grass_block", 2},
            {"minecraft:dirt", 3},
            {"minecraft:cobblestone", 4},
            {"minecraft:glass", 5},
            {"minecraft:gold_block", 6},
            {"minecraft:diamond_block", 7},
            {"minecraft:diamond_ore", 42},
            {"minecraft:deepslate_diamond_ore", 43},
        };
        const auto found = ids.find(type);
        if (found == ids.end()) return std::nullopt;
        return ResolvedBlockDescriptor{found->second, {type, states}};
    }
    std::optional<ChunkBuffer> captureChunk(const std::string &, ChunkPos pos) override {
        ++capture_count;
        if (failed_capture && *failed_capture == pos) return std::nullopt;
        if (failed_recapture && *failed_recapture == pos) {
            ++failed_recapture_capture_count;
            if (failed_recapture_capture_count > 1) return std::nullopt;
        }
        ChunkBuffer buffer(pos, 0, 15, 1);
        if (!incomplete_capture) buffer.setPaletteEntry(1, {"minecraft:stone", {}});
        buffer.setPaletteEntry(42, {"minecraft:diamond_ore", {}});
        if (existing_flat_y) {
            buffer.setPaletteEntry(2, {"minecraft:grass_block", {}});
            for (int x = 3; x <= 12; ++x) {
                for (int z = 3; z <= 12; ++z) {
                    buffer.setRuntimeId(x, *existing_flat_y, z, 2);
                }
            }
        }
        if (capture_count > 1 && (conflict_on_recapture || unrelated_change_on_recapture)) {
            buffer.setPaletteEntry(99, {"minecraft:external_edit", {}});
            if (conflict_on_recapture) buffer.setRuntimeId(1, 0, 1, 99);
            if (unrelated_change_on_recapture) buffer.setRuntimeId(2, 0, 2, 99);
        }
        return buffer;
    }
    bool commitChunk(const std::string &, const ChunkBuffer &buffer) override {
        committed = buffer; ++commit_count; return commit_succeeds;
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
    bool primary_thread{true};
    bool commit_succeeds{true};
    bool flush_succeeds{true};
    bool incomplete_capture{};
    bool conflict_on_recapture{};
    bool unrelated_change_on_recapture{};
    bool biome_edits_supported{};
    std::uint64_t intercepted{};
    int commit_count{};
    int flush_count{};
    int capture_count{};
    int failed_recapture_capture_count{};
    std::optional<ChunkPos> failed_capture;
    std::optional<ChunkPos> failed_recapture;
    std::optional<std::string> failed_descriptor;
    std::optional<int> existing_flat_y;
    std::optional<ChunkBuffer> committed;
    std::deque<NativeChunkRequest> requests;
};

int main() {
    assert(isSupportedBds2630Build("1.26.33"));
    assert(isSupportedBds2630Build("26.33"));
    assert(!isSupportedBds2630Build("1.26.32"));
    assert(!isSupportedBds2630Build("26.32"));
    assert(!isSupportedBds2630Build(""));
    assert(!isSupportedBds2630Build("1.26.30"));
    assert(!isSupportedBds2630Build("1.26.20"));
    assert(!isSupportedBds2630Build("1.26.34"));
    assert(!isSupportedBds2630Build("server-26.33"));
    assert(isExpectedBds2630Build("26.33", "1.26.33"));
    assert(!isExpectedBds2630Build("26.32", "1.26.33"));
    assert(isExpectedEndstoneVersion("0.11.6", "0.11.6"));
    assert(isExpectedEndstoneVersion("v0.11.6", "0.11.6"));
    assert(isExpectedEndstoneVersion("0.11.6+linux.x86-64", "0.11.6"));
    assert(isExpectedEndstoneVersion("0.11.6.dev7", "0.11.6"));
    assert(isExpectedEndstoneVersion("v0.11.6.dev7+linux", "v0.11.6"));
    assert(isExpectedEndstoneVersion("0.11.6-dev", "0.11.6"));
    assert(isExpectedEndstoneVersion("0.11.6-dev+linux", "0.11.6"));
    assert(isExpectedEndstoneVersion("0.11.6-dev.snapshot+linux", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.5", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.60", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6.1", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6-device", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6+", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6+linux..x64", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6.dev", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6.dev7+", "0.11.6"));
    assert(!isExpectedEndstoneVersion("0.11.6-dev.", "0.11.6"));

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

    // Manual live recipes must use descriptors resolved by the exact adapter,
    // preserve every cell outside their bounded patch, and confirm both commit
    // and flush before reporting a successful live change.
    MockNativeAdapter live_adapter;
    const auto live_flat = generateLive(live_adapter, "overworld", {0, 0}, 8, "flat");
    assert(live_flat.success);
    assert(live_flat.committed);
    assert(live_flat.failure == LiveGenerationFailure::None);
    assert(live_flat.requested_chunks == 1);
    assert(live_flat.planned_blocks == 100);
    assert(live_flat.changed_blocks == 100);
    assert(live_flat.changed_chunks == 1);
    assert(live_flat.unconfirmed_blocks == 0);
    assert(live_flat.has_changed_y_range);
    assert(live_flat.min_changed_y == 8);
    assert(live_flat.max_changed_y == 8);
    assert(live_adapter.commit_count == 1);
    assert(live_adapter.flush_count == 1);
    assert(live_adapter.committed.has_value());
    assert(live_adapter.committed->getRuntimeId(3, 8, 3) == 2);
    assert(live_adapter.committed->getRuntimeId(0, 8, 0) == 1);
    assert(live_adapter.committed->biomeCellCount() == 0);

    MockNativeAdapter no_change_adapter;
    no_change_adapter.existing_flat_y = 8;
    const auto no_change = generateLive(
        no_change_adapter, "overworld", {0, 0}, 8, "flat");
    assert(no_change.success);
    assert(!no_change.committed);
    assert(no_change.planned_blocks == 0);
    assert(no_change.changed_blocks == 0);
    assert(!no_change.has_changed_y_range);
    assert(no_change_adapter.commit_count == 0);

    MockNativeAdapter structure_adapter;
    const auto live_castle = generateLive(structure_adapter, "overworld", {10, -10}, 8, "castle");
    assert(live_castle.success);
    assert(live_castle.committed);
    assert(live_castle.requested_chunks == 9);
    assert(live_castle.changed_chunks == 9);
    assert(structure_adapter.capture_count == 9);
    assert(structure_adapter.commit_count == 9);
    assert(structure_adapter.flush_count == 9);
    assert(live_castle.has_changed_y_range);
    assert(live_castle.min_changed_y == 8);
    assert(live_castle.max_changed_y == 15);

    MockNativeAdapter wrong_thread_adapter;
    wrong_thread_adapter.primary_thread = false;
    const auto wrong_thread = generateLive(wrong_thread_adapter, "overworld", {0, 0}, 8, "flat");
    assert(!wrong_thread.success);
    assert(!wrong_thread.committed);
    assert(wrong_thread.failure == LiveGenerationFailure::WrongThread);
    assert(wrong_thread_adapter.capture_count == 0);

    MockNativeAdapter descriptor_adapter;
    descriptor_adapter.failed_descriptor = "minecraft:grass_block";
    const auto descriptor_failure = generateLive(descriptor_adapter, "overworld", {0, 0}, 8, "flat");
    assert(!descriptor_failure.success);
    assert(descriptor_failure.failure == LiveGenerationFailure::DescriptorResolution);
    assert(descriptor_adapter.capture_count == 0);

    MockNativeAdapter live_capture_adapter;
    live_capture_adapter.failed_capture = ChunkPos{0, 0};
    const auto live_capture_failure = generateLive(live_capture_adapter, "overworld", {0, 0}, 8, "flat");
    assert(!live_capture_failure.success);
    assert(live_capture_failure.failure == LiveGenerationFailure::Capture);
    assert(live_capture_failure.message.find("must already be loaded") != std::string::npos);
    assert(live_capture_adapter.commit_count == 0);

    MockNativeAdapter incomplete_adapter;
    incomplete_adapter.incomplete_capture = true;
    const auto incomplete_failure = generateLive(incomplete_adapter, "overworld", {0, 0}, 8, "flat");
    assert(!incomplete_failure.success);
    assert(incomplete_failure.failure == LiveGenerationFailure::IncompletePalette);
    assert(incomplete_adapter.commit_count == 0);

    MockNativeAdapter live_commit_adapter;
    live_commit_adapter.commit_succeeds = false;
    const auto live_commit_failure = generateLive(live_commit_adapter, "overworld", {0, 0}, 8, "flat");
    assert(!live_commit_failure.success);
    assert(!live_commit_failure.committed);
    assert(live_commit_failure.failure == LiveGenerationFailure::Commit);
    assert(live_commit_failure.changed_blocks == 0);
    assert(live_commit_adapter.flush_count == 0);

    MockNativeAdapter live_flush_adapter;
    live_flush_adapter.flush_succeeds = false;
    const auto live_flush_failure = generateLive(live_flush_adapter, "overworld", {0, 0}, 8, "flat");
    assert(!live_flush_failure.success);
    assert(!live_flush_failure.committed);
    assert(live_flush_failure.failure == LiveGenerationFailure::Flush);
    assert(live_flush_failure.changed_blocks == 0);
    assert(live_flush_failure.unconfirmed_blocks == live_flush_failure.planned_blocks);
    assert(live_flush_failure.has_changed_y_range);
    assert(live_flush_failure.min_changed_y == 8);
    assert(live_flush_failure.max_changed_y == 8);

    const auto invalid_live_recipe = generateLive(live_adapter, "overworld", {0, 0}, 8, "unknown");
    assert(!invalid_live_recipe.success);
    assert(invalid_live_recipe.failure == LiveGenerationFailure::InvalidRecipe);

    MockNativeAdapter low_anchor_adapter;
    const auto low_anchor = generateLive(low_anchor_adapter, "overworld", {0, 0}, 0, "flat");
    assert(!low_anchor.success);
    assert(low_anchor.failure == LiveGenerationFailure::UnsupportedAdapter);
    assert(low_anchor_adapter.capture_count == 1);
    assert(low_anchor_adapter.commit_count == 0);
    assert(!low_anchor.has_changed_y_range);

    MockNativeAdapter high_structure_anchor_adapter;
    const auto high_structure_anchor = generateLive(
        high_structure_anchor_adapter, "overworld", {0, 0}, 9, "castle");
    assert(!high_structure_anchor.success);
    assert(high_structure_anchor.failure == LiveGenerationFailure::UnsupportedAdapter);
    assert(high_structure_anchor_adapter.capture_count == 9);
    assert(high_structure_anchor_adapter.commit_count == 0);
    assert(!high_structure_anchor.has_changed_y_range);

    MockNativeAdapter ores_adapter;
    const auto live_ores = generateLive(
        ores_adapter, "overworld", {0, 0}, 1'000'000, "ores");
    assert(live_ores.success);
    assert(live_ores.committed);
    assert(live_ores.has_changed_y_range);
    assert(live_ores.min_changed_y >= 1);
    assert(live_ores.max_changed_y <= 14);

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

    // The standalone API normally starts before consumer populators register.
    // Empty intercepted work must not capture or commit; manual recipes remain
    // available through their separate synchronous service path.
    MockNativeAdapter zero_populator_adapter;
    GenerationInterceptor zero_populator(zero_populator_adapter, scheduler, 9876);
    assert(zero_populator.install());
    zero_populator_adapter.push({8, 10});
    const auto zero_stats = zero_populator.pump();
    assert(zero_stats.native_requests == 1);
    assert(zero_stats.empty_pipeline_requests == 1);
    assert(zero_stats.waiting == 0);
    assert(zero_stats.inflight == 0);
    assert(zero_populator_adapter.capture_count == 0);
    assert(zero_populator_adapter.commit_count == 0);
    assert(zero_populator_adapter.flush_count == 0);
    const auto zero_manual = generateLive(
        zero_populator_adapter, "overworld", {8, 10}, 8, "maze");
    assert(zero_manual.success);
    assert(zero_manual.committed);
    assert(zero_populator_adapter.commit_count == 1);
    assert(zero_populator_adapter.flush_count == 1);
    zero_populator.disable();

    // A no-op populator must not recapture, full-commit, or flush its stale
    // snapshot merely because it ran in the automatic pipeline.
    MockNativeAdapter no_op_adapter;
    GenerationInterceptor no_op_interceptor(no_op_adapter, scheduler, 9876);
    no_op_interceptor.addPopulator(std::make_shared<NoOpPop>());
    assert(no_op_interceptor.install());
    no_op_adapter.push({9, 11});
    InterceptorStats no_op_stats;
    for (int i = 0; i < 200; ++i) {
        no_op_stats = no_op_interceptor.pump();
        if (no_op_stats.dispatched == 1 && no_op_stats.inflight == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(no_op_stats.dispatched == 1);
    assert(no_op_stats.inflight == 0);
    assert(no_op_stats.commit_failures == 0);
    assert(no_op_adapter.capture_count == 1);
    assert(no_op_adapter.commit_count == 0);
    assert(no_op_adapter.flush_count == 0);
    no_op_interceptor.disable();

    // Revalidate only cells changed by the worker. A conflict aborts before
    // commit, while an unrelated live edit survives in the merged candidate.
    MockNativeAdapter conflict_adapter;
    conflict_adapter.conflict_on_recapture = true;
    GenerationInterceptor conflict_interceptor(conflict_adapter, scheduler, 9876);
    conflict_interceptor.addPopulator(std::make_shared<FixedPop>());
    assert(conflict_interceptor.install());
    conflict_adapter.push({10, 12});
    InterceptorStats conflict_stats;
    for (int i = 0; i < 200; ++i) {
        conflict_stats = conflict_interceptor.pump();
        if (conflict_stats.dispatched == 1 && conflict_stats.inflight == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(conflict_stats.commit_failures == 1);
    assert(conflict_adapter.capture_count == 2);
    assert(conflict_adapter.commit_count == 0);
    assert(conflict_adapter.flush_count == 0);
    conflict_interceptor.disable();

    MockNativeAdapter recapture_adapter;
    recapture_adapter.failed_recapture = ChunkPos{10, 13};
    InterceptorConfig recapture_config;
    recapture_config.capture_retry_ticks = 3;
    GenerationInterceptor recapture_interceptor(
        recapture_adapter, scheduler, 9876, recapture_config);
    recapture_interceptor.addPopulator(std::make_shared<FixedPop>());
    assert(recapture_interceptor.install());
    recapture_adapter.push({10, 13});
    InterceptorStats recapture_stats;
    for (int i = 0; i < 200; ++i) {
        recapture_stats = recapture_interceptor.pump();
        if (recapture_stats.dispatched == 1 && recapture_stats.inflight == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(recapture_stats.capture_retries == 3);
    assert(recapture_stats.capture_failures == 1);
    assert(recapture_stats.commit_failures == 1);
    assert(recapture_adapter.capture_count == 4);
    assert(recapture_adapter.commit_count == 0);
    assert(recapture_adapter.flush_count == 0);
    recapture_interceptor.disable();

    // A completed job waiting on recapture must rotate behind its ready peers
    // instead of monopolizing the one-item commit budget for every retry tick.
    MockNativeAdapter fair_retry_adapter;
    fair_retry_adapter.failed_recapture = ChunkPos{15, 17};
    InterceptorConfig fair_retry_config;
    fair_retry_config.max_captures_per_pump = 2;
    fair_retry_config.max_commits_per_pump = 1;
    fair_retry_config.max_inflight = 2;
    fair_retry_config.capture_retry_ticks = 3;
    // A single worker plus a same-priority sentinel gives this test a real
    // future-readiness barrier. The populator's completion counter alone is
    // too early: the scheduler still fingerprints the chunk before fulfilling
    // the corresponding packaged-task future.
    GenerationScheduler fair_retry_scheduler(1);
    GenerationInterceptor fair_retry_interceptor(
        fair_retry_adapter, fair_retry_scheduler, 9876, fair_retry_config);
    std::atomic<int> fair_retry_completed{};
    fair_retry_interceptor.addPopulator(
        std::make_shared<CountingPop>(fair_retry_completed));
    assert(fair_retry_interceptor.install());
    fair_retry_adapter.push({15, 17});
    fair_retry_adapter.push({16, 18});
    auto fair_retry_stats = fair_retry_interceptor.pump();
    assert(fair_retry_stats.dispatched == 2);
    auto readiness_barrier = fair_retry_scheduler.populatePipeline(
        {9876, "overworld", {17, 19}, GenerationStage::Decoration, 0},
        ChunkBuffer({17, 19}, 0, 15, 1), {}, fair_retry_config.priority);
    (void)readiness_barrier.get();
    assert(fair_retry_completed.load(std::memory_order_acquire) == 2);
    fair_retry_stats = fair_retry_interceptor.pump();
    assert(fair_retry_adapter.commit_count == 0);
    assert(fair_retry_stats.capture_retries == 1);
    assert(fair_retry_stats.capture_failures == 0);
    fair_retry_stats = fair_retry_interceptor.pump();
    assert(fair_retry_adapter.commit_count == 1);
    assert(fair_retry_adapter.committed.has_value());
    assert((fair_retry_adapter.committed->position() == ChunkPos{16, 18}));
    assert(fair_retry_stats.capture_retries == 1);
    assert(fair_retry_stats.capture_failures == 0);
    assert(fair_retry_stats.commit_failures == 0);
    fair_retry_interceptor.disable();

    MockNativeAdapter merge_adapter;
    merge_adapter.unrelated_change_on_recapture = true;
    GenerationInterceptor merge_interceptor(merge_adapter, scheduler, 9876);
    merge_interceptor.addPopulator(std::make_shared<FixedPop>());
    assert(merge_interceptor.install());
    merge_adapter.push({11, 13});
    InterceptorStats merge_stats;
    for (int i = 0; i < 200; ++i) {
        merge_stats = merge_interceptor.pump();
        if (merge_stats.committed == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(merge_stats.commit_failures == 0);
    assert(merge_adapter.capture_count == 2);
    assert(merge_adapter.commit_count == 1);
    assert(merge_adapter.flush_count == 1);
    assert(merge_adapter.committed.has_value());
    assert(merge_adapter.committed->getRuntimeId(1, 0, 1) == 77);
    assert(merge_adapter.committed->paletteEntry(77) != nullptr);
    assert(merge_adapter.committed->paletteEntry(77)->type == "minecraft:new_test_block");
    assert(merge_adapter.committed->getRuntimeId(2, 0, 2) == 99);
    merge_interceptor.disable();

    MockNativeAdapter missing_palette_adapter;
    GenerationInterceptor missing_palette_interceptor(
        missing_palette_adapter, scheduler, 9876);
    missing_palette_interceptor.addPopulator(std::make_shared<MissingPalettePop>());
    assert(missing_palette_interceptor.install());
    missing_palette_adapter.push({12, 14});
    InterceptorStats missing_palette_stats;
    for (int i = 0; i < 200; ++i) {
        missing_palette_stats = missing_palette_interceptor.pump();
        if (missing_palette_stats.dispatched == 1 &&
            missing_palette_stats.inflight == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(missing_palette_stats.commit_failures == 1);
    assert(missing_palette_adapter.commit_count == 0);
    assert(missing_palette_adapter.flush_count == 0);
    missing_palette_interceptor.disable();

    MockNativeAdapter biome_adapter;
    biome_adapter.biome_edits_supported = true;
    GenerationInterceptor biome_interceptor(biome_adapter, scheduler, 9876);
    biome_interceptor.addPopulator(std::make_shared<BiomePop>());
    assert(biome_interceptor.install());
    biome_adapter.push({13, 15});
    InterceptorStats biome_stats;
    for (int i = 0; i < 200; ++i) {
        biome_stats = biome_interceptor.pump();
        if (biome_stats.committed == 1) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(biome_stats.commit_failures == 0);
    assert(biome_adapter.committed.has_value());
    assert(biome_adapter.committed->biomeCells().at(0) == 7);
    biome_interceptor.disable();

    MockNativeAdapter rejected_biome_adapter;
    GenerationInterceptor rejected_biome_interceptor(
        rejected_biome_adapter, scheduler, 9876);
    rejected_biome_interceptor.addPopulator(std::make_shared<BiomePop>());
    assert(rejected_biome_interceptor.install());
    rejected_biome_adapter.push({14, 16});
    InterceptorStats rejected_biome_stats;
    for (int i = 0; i < 200; ++i) {
        rejected_biome_stats = rejected_biome_interceptor.pump();
        if (rejected_biome_stats.dispatched == 1 &&
            rejected_biome_stats.inflight == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(rejected_biome_stats.commit_failures == 1);
    assert(rejected_biome_adapter.commit_count == 0);
    rejected_biome_interceptor.disable();

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
        const auto captures_before_pump = commit_adapter.capture_count;
        commit_stats = commit_limited.pump();
        assert(commit_adapter.commit_count - before_pump <= 1);
        assert(commit_adapter.capture_count - captures_before_pump <= 1);
        if (commit_adapter.commit_count < 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(commit_adapter.commit_count == 2);
    commit_adapter.flush_succeeds = false;
    for (int i = 0; i < 200 && commit_adapter.commit_count < 3; ++i) {
        const auto before_pump = commit_adapter.commit_count;
        const auto captures_before_pump = commit_adapter.capture_count;
        commit_stats = commit_limited.pump();
        assert(commit_adapter.commit_count - before_pump <= 1);
        assert(commit_adapter.capture_count - captures_before_pump <= 1);
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
