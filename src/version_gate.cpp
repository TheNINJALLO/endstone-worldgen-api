#include "endstone_worldgen/bds_26_30_adapter.h"

namespace endstone_worldgen {
bool isSupportedBds2630Build(std::string_view build) noexcept {
    if (build.empty()) return true;
    return build.find("1.26.3") != std::string_view::npos ||
           build.find("26.3") != std::string_view::npos;
}
}
