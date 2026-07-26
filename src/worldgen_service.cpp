#include "endstone_worldgen/worldgen_service.h"

#include <utility>

namespace endstone_worldgen {

WorldGenServiceProvider::WorldGenServiceProvider(
    IVanillaGenerationAdapter &adapter, GenerationInterceptor &interceptor)
    : adapter_(adapter), interceptor_(interceptor) {}

void WorldGenServiceProvider::registerPopulator(std::shared_ptr<IPopulator> populator) {
    interceptor_.addPopulator(std::move(populator));
}

void WorldGenServiceProvider::clearPopulators() { interceptor_.clearPopulators(); }

std::size_t WorldGenServiceProvider::populatorCount() const {
    return interceptor_.populatorCount();
}

InterceptorStats WorldGenServiceProvider::stats() const { return interceptor_.stats(); }

NativeDiagnostics WorldGenServiceProvider::diagnostics() const { return adapter_.diagnostics(); }

bool WorldGenServiceProvider::interceptionActive() const {
    return adapter_.diagnostics().interception_installed;
}

LiveGenerationResult WorldGenServiceProvider::generateLive(
    const std::string &dimension, ChunkPos center, std::int32_t anchor_y,
    const std::string &recipe) {
    auto result = endstone_worldgen::generateLive(
        adapter_, dimension, center, anchor_y, recipe);
    std::scoped_lock lock(live_stats_mutex_);
    ++live_stats_.requests;
    live_stats_.changed_blocks += result.changed_blocks;
    live_stats_.unconfirmed_blocks += result.unconfirmed_blocks;
    if (result.success) {
        ++live_stats_.successful_requests;
        if (result.committed) ++live_stats_.committed_requests;
        else ++live_stats_.no_change_requests;
        return result;
    }

    ++live_stats_.failed_requests;
    switch (result.failure) {
    case LiveGenerationFailure::DescriptorResolution:
        ++live_stats_.descriptor_failures;
        break;
    case LiveGenerationFailure::Capture:
    case LiveGenerationFailure::IncompletePalette:
        ++live_stats_.capture_failures;
        break;
    case LiveGenerationFailure::Commit:
        ++live_stats_.commit_failures;
        break;
    case LiveGenerationFailure::Flush:
        ++live_stats_.flush_failures;
        break;
    case LiveGenerationFailure::WrongThread:
        ++live_stats_.primary_thread_rejections;
        break;
    default:
        break;
    }
    return result;
}

LiveGenerationStats WorldGenServiceProvider::liveStats() const {
    std::scoped_lock lock(live_stats_mutex_);
    return live_stats_;
}

} // namespace endstone_worldgen
