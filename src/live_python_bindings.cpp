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
        result["diagnostics"] = py::dict();
        return result;
    }

    result["interception_active"] = service->interceptionActive();
    result["populator_count"] = service->populatorCount();
    result["stats"] = statsToDict(service->stats());
    result["diagnostics"] = diagnosticsToDict(service->diagnostics());
    return result;
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
               "Return whether the native endstone:worldgen service is registered.");
    module.def("status", &endstone_worldgen::status, py::arg("server"),
               "Return live interceptor statistics and native diagnostics.");
    module.def("clear_populators", &endstone_worldgen::clearPopulators, py::arg("server"),
               "Clear native populators; return false when the service is unavailable.");
}
