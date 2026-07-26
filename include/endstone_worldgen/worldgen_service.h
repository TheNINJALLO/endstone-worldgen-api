#pragma once
#include "endstone_worldgen/generation_interceptor.h"
#include "endstone_worldgen/live_generation.h"
#include <endstone/plugin/service.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace endstone_worldgen {
// Service ABI 2 adds synchronous live recipes and statistics. A versioned
// lookup prevents a bridge built for a different virtual interface from
// calling the wrong vtable slot across the DSO boundary.
inline constexpr std::uint32_t WorldGenServiceAbiVersion = 2;
inline constexpr std::string_view WorldGenServiceName = "endstone:worldgen:v2";

struct LiveGenerationStats {
    std::uint64_t requests{};
    std::uint64_t successful_requests{};
    std::uint64_t committed_requests{};
    std::uint64_t no_change_requests{};
    std::uint64_t failed_requests{};
    std::uint64_t changed_blocks{};
    std::uint64_t unconfirmed_blocks{};
    std::uint64_t descriptor_failures{};
    std::uint64_t capture_failures{};
    std::uint64_t commit_failures{};
    std::uint64_t flush_failures{};
    std::uint64_t primary_thread_rejections{};
};

// Service registered in Endstone's ServiceManager. Other native plugins can load
// this interface and register worker-safe populators without linking to the
// concrete plugin implementation.
class WorldGenService : public endstone::Service {
public:
    ~WorldGenService() override = default;
    virtual void registerPopulator(std::shared_ptr<IPopulator> populator) = 0;
    virtual void clearPopulators() = 0;
    [[nodiscard]] virtual std::size_t populatorCount() const = 0;
    [[nodiscard]] virtual InterceptorStats stats() const = 0;
    [[nodiscard]] virtual NativeDiagnostics diagnostics() const = 0;
    [[nodiscard]] virtual bool interceptionActive() const = 0;
    [[nodiscard]] virtual LiveGenerationResult generateLive(
        const std::string &dimension, ChunkPos center, std::int32_t anchor_y,
        const std::string &recipe) = 0;
    [[nodiscard]] virtual LiveGenerationStats liveStats() const = 0;
};

class WorldGenServiceProvider final : public WorldGenService {
public:
    WorldGenServiceProvider(IVanillaGenerationAdapter &adapter, GenerationInterceptor &interceptor);
    void registerPopulator(std::shared_ptr<IPopulator> populator) override;
    void clearPopulators() override;
    [[nodiscard]] std::size_t populatorCount() const override;
    [[nodiscard]] InterceptorStats stats() const override;
    [[nodiscard]] NativeDiagnostics diagnostics() const override;
    [[nodiscard]] bool interceptionActive() const override;
    [[nodiscard]] LiveGenerationResult generateLive(
        const std::string &dimension, ChunkPos center, std::int32_t anchor_y,
        const std::string &recipe) override;
    [[nodiscard]] LiveGenerationStats liveStats() const override;
private:
    IVanillaGenerationAdapter &adapter_;
    GenerationInterceptor &interceptor_;
    mutable std::mutex live_stats_mutex_;
    LiveGenerationStats live_stats_;
};
}
