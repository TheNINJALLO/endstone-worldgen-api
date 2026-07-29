#pragma once
#include "endstone_worldgen/native_adapter.h"
#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace endstone_worldgen {
struct InterceptorConfig {
    std::size_t max_requests_per_pump{16};
    std::size_t max_captures_per_pump{1};
    std::size_t max_commits_per_pump{1};
    std::size_t max_inflight{32};
    std::size_t max_waiting{256};
    std::size_t capture_retry_ticks{40};
    bool intercept_get_or_load{false};
    JobPriority priority{JobPriority::ViewDistance};
};

struct InterceptorStats {
    std::uint64_t native_requests{};
    std::uint64_t load_requests_ignored{};
    std::uint64_t deduplicated{};
    std::uint64_t dispatched{};
    std::uint64_t committed{};
    std::uint64_t capture_retries{};
    std::uint64_t capture_failures{};
    std::uint64_t commit_failures{};
    std::uint64_t empty_pipeline_requests{};
    std::uint64_t waiting_overflow_drops{};
    std::size_t inflight{};
    std::size_t waiting{};
};

class GenerationInterceptor {
public:
    GenerationInterceptor(IVanillaGenerationAdapter &adapter, GenerationScheduler &scheduler,
                          std::uint64_t world_seed, InterceptorConfig config = {});
    ~GenerationInterceptor();

    bool install();
    void disable() noexcept;
    void addPopulator(std::shared_ptr<IPopulator> populator);
    void clearPopulators();
    [[nodiscard]] std::size_t populatorCount() const;

    // Must be called from the BDS/Endstone primary thread. It drains native hook
    // requests, captures detached chunks within a strict per-pump budget,
    // dispatches worker jobs, then commits completed results through the
    // adapter's separately bounded primary-thread gate.
    InterceptorStats pump();
    [[nodiscard]] InterceptorStats stats() const;

private:
    struct WaitingRequest { NativeChunkRequest request; std::size_t attempts{}; };
    struct PendingJob {
        NativeChunkRequest request;
        ChunkBuffer baseline;
        std::future<GenerationResult> future;
        std::optional<GenerationResult> completed;
        std::size_t recapture_attempts{};
    };
    [[nodiscard]] std::string keyOf(const NativeChunkRequest &request) const;
    void ingestRequests();
    void dispatchWaiting();
    void commitReady();

    IVanillaGenerationAdapter &adapter_;
    GenerationScheduler &scheduler_;
    std::uint64_t world_seed_{};
    InterceptorConfig config_;
    std::vector<std::shared_ptr<IPopulator>> populators_;
    std::deque<WaitingRequest> waiting_;
    std::vector<PendingJob> pending_;
    std::unordered_set<std::string> active_keys_;
    mutable std::mutex mutex_;
    InterceptorStats stats_;
    bool installed_{};
};
}
