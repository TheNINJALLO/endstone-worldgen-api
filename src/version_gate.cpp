#include "endstone_worldgen/bds_26_30_adapter.h"
namespace endstone_worldgen {
bool isSupportedBds2630Build(std::string_view build) noexcept {
    return build == "1.26.32" || build == "1.26.33";
}
}
