#include "endstone_worldgen/worldgen_service.h"

#include <endstone/plugin/service_manager.h>
#include <endstone/server.h>
#include <pybind11/pybind11.h>

#include <memory>
#include <string>

namespace py = pybind11;

namespace endstone_worldgen {
namespace {
std::shared_ptr<WorldGenService> loadService(endstone::Server &server) {
    return server.getServiceManager().load<WorldGenService>(std::string(WorldGenServiceName));
}

py::dict statsToDict(const InterceptorStats &stats) {
    py::dict result;
    result["native_requests"] = stats.native_requests;
    result["load_requests_ignored"] = stats.load_requests_ignored;
    result["deduplicated"] = stats.deduplicated;
    result["dispatched"] = stats.dispatched;
    result["committed"] = stats.committed;
    result["capture_retries"] = stats.capture_retries;
    result["capture_failures"] = stats.capture_failures;
    result["commit_failures"] = stats.commit_failures;
    result["empty_pipeline_requests"] = stats.empty_pipeline_requests;
    result["waiting_overflow_drops"] = stats.waiting_overflow_drops;
    result["inflight"] = stats.inflight;
    result["waiting"] = stats.waiting;
    return result;
}

py::dict diagnosticsToDict(const NativeDiagnostics &diagnostics) {
    py::dict result;
    result["adapter"] = diagnostics.adapter;
    result["runtime_build"] = diagnostics.runtime_build;
    result["runtime_endstone_version"] = diagnostics.runtime_endstone_version;
    result["exact_build_match"] = diagnostics.exact_build_match;
    result["primary_thread"] = diagnostics.primary_thread;
    result["chunk_source_available"] = diagnostics.chunk_source_available;
    result["chunk_source_can_launch_tasks"] = diagnostics.chunk_source_can_launch_tasks;
    result["interception_installed"] = diagnostics.interception_installed;
    result["hooked_chunk_sources"] = diagnostics.hooked_chunk_sources;
    result["intercepted_requests"] = diagnostics.intercepted_requests;
    result["dropped_requests"] = diagnostics.dropped_requests;
    return result;
}

py::dict liveStatsToDict(const LiveGenerationStats &stats) {
    py::dict result;
    result["requests"] = stats.requests;
    result["successful_requests"] = stats.successful_requests;
    result["committed_requests"] = stats.committed_requests;
    result["no_change_requests"] = stats.no_change_requests;
    result["failed_requests"] = stats.failed_requests;
    result["changed_blocks"] = stats.changed_blocks;
    result["unconfirmed_blocks"] = stats.unconfirmed_blocks;
    result["descriptor_failures"] = stats.descriptor_failures;
    result["capture_failures"] = stats.capture_failures;
    result["commit_failures"] = stats.commit_failures;
    result["flush_failures"] = stats.flush_failures;
    result["primary_thread_rejections"] = stats.primary_thread_rejections;
    return result;
}

py::dict liveResultToDict(const LiveGenerationResult &live) {
    py::dict result;
    result["success"] = live.success;
    result["committed"] = live.committed;
    result["failure"] = std::string(liveGenerationFailureName(live.failure));
    result["requested_chunks"] = live.requested_chunks;
    result["planned_blocks"] = live.planned_blocks;
    result["changed_blocks"] = live.changed_blocks;
    result["changed_chunks"] = live.changed_chunks;
    result["unconfirmed_blocks"] = live.unconfirmed_blocks;
    if (live.has_changed_y_range) {
        result["min_changed_y"] = live.min_changed_y;
        result["max_changed_y"] = live.max_changed_y;
    } else {
        result["min_changed_y"] = py::none();
        result["max_changed_y"] = py::none();
    }
    result["message"] = live.message;
    return result;
}

bool available(endstone::Server &server) noexcept {
    try {
        return static_cast<bool>(loadService(server));
    } catch (...) {
        return false;
    }
}

py::dict status(endstone::Server &server) {
    py::dict result;
    const auto service = loadService(server);
    result["available"] = static_cast<bool>(service);
    if (!service) {
        result["interception_active"] = false;
        result["populator_count"] = 0;
        result["stats"] = py::dict();
        result["live_stats"] = py::dict();
        result["diagnostics"] = py::dict();
        return result;
    }

    result["interception_active"] = service->interceptionActive();
    result["populator_count"] = service->populatorCount();
    result["stats"] = statsToDict(service->stats());
    result["live_stats"] = liveStatsToDict(service->liveStats());
    result["diagnostics"] = diagnosticsToDict(service->diagnostics());
    return result;
}

py::dict generateLiveCommand(endstone::Server &server, const std::string &dimension,
                             std::int32_t chunk_x, std::int32_t chunk_z,
                             std::int32_t anchor_y,
                             const std::string &recipe) {
    const auto service = loadService(server);
    if (!service) {
        py::dict result;
        result["success"] = false;
        result["committed"] = false;
        result["failure"] = "service_unavailable";
        result["requested_chunks"] = 0;
        result["planned_blocks"] = 0;
        result["changed_blocks"] = 0;
        result["changed_chunks"] = 0;
        result["unconfirmed_blocks"] = 0;
        result["min_changed_y"] = py::none();
        result["max_changed_y"] = py::none();
        result["message"] = "the native endstone:worldgen:v2 service is unavailable";
        return result;
    }
    return liveResultToDict(service->generateLive(
        dimension, {chunk_x, chunk_z}, anchor_y, recipe));
}

bool clearPopulators(endstone::Server &server) {
    const auto service = loadService(server);
    if (!service) return false;
    service->clearPopulators();
    return true;
}
} // namespace
} // namespace endstone_worldgen

PYBIND11_MODULE(_endstone_worldgen_live, module) {
    module.doc() = "Live bridge to the loaded Endstone WorldGen native service";
    module.def("available", &endstone_worldgen::available, py::arg("server"),
               "Return whether the native endstone:worldgen:v2 service is registered.");
    module.def("status", &endstone_worldgen::status, py::arg("server"),
               "Return live interceptor statistics and native diagnostics.");
    module.def("generate_live", &endstone_worldgen::generateLiveCommand,
               py::arg("server"), py::arg("dimension"), py::arg("chunk_x"),
               py::arg("chunk_z"), py::arg("anchor_y"), py::arg("recipe"),
               "Run a bounded built-in recipe through primary-thread capture, commit and flush.");
    module.def("clear_populators", &endstone_worldgen::clearPopulators, py::arg("server"),
               "Clear native populators; return false when the service is unavailable.");
}
