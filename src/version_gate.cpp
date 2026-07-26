#include "endstone_worldgen/bds_26_30_adapter.h"

namespace endstone_worldgen {
namespace {
std::string_view canonicalBdsBuild(std::string_view build) noexcept {
    if (build.starts_with("1.")) build.remove_prefix(2);
    return build;
}
} // namespace

bool isSupportedBds2630Build(std::string_view build) noexcept {
    build = canonicalBdsBuild(build);
    return build == "26.33";
}

bool isExpectedBds2630Build(std::string_view runtime_build,
                            std::string_view packaged_build) noexcept {
    if (!isSupportedBds2630Build(runtime_build) ||
        !isSupportedBds2630Build(packaged_build)) {
        return false;
    }
    return canonicalBdsBuild(runtime_build) == canonicalBdsBuild(packaged_build);
}

bool isExpectedEndstoneVersion(std::string_view runtime_version,
                               std::string_view packaged_version) noexcept {
    if (runtime_version.starts_with('v')) runtime_version.remove_prefix(1);
    if (packaged_version.starts_with('v')) packaged_version.remove_prefix(1);
    return !runtime_version.empty() && runtime_version == packaged_version;
}
}
