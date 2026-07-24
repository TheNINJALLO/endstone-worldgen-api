#include <format>
namespace std {
#if defined(_LIBCPP_VERSION)
inline namespace __1 {
typedef ::std::format_context format_context;
}
#endif
typedef ::std::format_context format_context;
}

#include <endstone/endstone.hpp>
#include <endstone/plugin/service_manager.h>
#include <endstone/plugin/service_priority.h>
#include "endstone_worldgen/bds_26_30_adapter.h"
#include "endstone_worldgen/generation_interceptor.h"
#include "endstone_worldgen/generation_scheduler.h"
#include "endstone_worldgen/worldgen_service.h"
#include "version.h"
#include <algorithm>
#include <memory>
#include <string>
#include <thread>

class WorldGenPlugin : public endstone::Plugin {
public:
    void onEnable() override {
        const auto hardware = std::thread::hardware_concurrency();
        const auto workers = hardware > 3 ? hardware - 2 : 1;
        scheduler_ = std::make_unique<endstone_worldgen::GenerationScheduler>(workers);
#if ENDSTONE_WORLDGEN_NATIVE_2630
        adapter_ = endstone_worldgen::makeBds2630WorldGenAdapter(getServer());
#endif
        getLogger().info("WorldGen API {} enabled with {} detached workers; BDS={}",
                         ENDSTONE_WORLDGEN_VERSION, scheduler_->workerCount(), getServer().getMinecraftVersion());

        if (!adapter_) {
            getLogger().error("Exact 26.30 adapter refused this runtime. Chunk interception, capture and commit are disabled.");
            return;
        }

        endstone_worldgen::InterceptorConfig config;
        config.max_requests_per_pump = 24;
        config.max_commits_per_pump = 2;
        config.max_inflight = std::max<std::size_t>(8, scheduler_->workerCount() * 2);
        config.capture_retry_ticks = 40;
        config.intercept_get_or_load = false;
        interceptor_ = std::make_unique<endstone_worldgen::GenerationInterceptor>(
            *adapter_, *scheduler_, static_cast<std::uint64_t>(getServer().getLevel()->getSeed()), config);

        if (!interceptor_->install()) {
            getLogger().error("The exact ChunkSource hook could not be installed. Worker dispatch will not run.");
            interceptor_.reset();
            adapter_.reset();
            return;
        }

        provider_ = std::make_shared<endstone_worldgen::WorldGenServiceProvider>(*adapter_, *interceptor_);
        getServer().getServiceManager().registerService(
            std::string(endstone_worldgen::WorldGenServiceName), provider_, *this,
            endstone::ServicePriority::Normal);

        getServer().getScheduler().runTaskTimer(*this, [this]() { pump(); }, 1, 1);
    }

    void onDisable() override {
        getServer().getScheduler().cancelTasks(*this);
        getServer().getServiceManager().unregisterAll(*this);
        provider_.reset();
        if (interceptor_) interceptor_->disable();
        interceptor_.reset();
        adapter_.reset();
        scheduler_.reset();
    }

private:
    void pump() {
        if (!interceptor_) return;
        const auto stats = interceptor_->pump();
        if (++ticks_ % 1200 == 0) {
            const auto diagnostics = adapter_->diagnostics();
            getLogger().info("intercepted={} dispatched={} committed={} waiting={} inflight={} retries={} dropped={} populators={}",
                             diagnostics.intercepted_requests, stats.dispatched, stats.committed,
                             stats.waiting, stats.inflight, stats.capture_retries,
                             diagnostics.dropped_requests, interceptor_->populatorCount());
        }
    }

    std::unique_ptr<endstone_worldgen::GenerationScheduler> scheduler_;
    std::unique_ptr<endstone_worldgen::IVanillaGenerationAdapter> adapter_;
    std::unique_ptr<endstone_worldgen::GenerationInterceptor> interceptor_;
    std::shared_ptr<endstone_worldgen::WorldGenServiceProvider> provider_;
    std::uint64_t ticks_{};
};

ENDSTONE_PLUGIN("endstone_worldgen", ENDSTONE_WORLDGEN_VERSION, WorldGenPlugin) {
    prefix = "WorldGen";
    description = "Hooked detached world-generation and population service for Endstone";
    website = "https://github.com/TheNINJALLO/endstone-worldgen-api";
    authors = {"Ninj-OS contributors"};
}
