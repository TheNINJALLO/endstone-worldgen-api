#pragma once
#include "endstone_worldgen/native_adapter.h"
#include <memory>
#include <string_view>
namespace endstone { class Server; }
namespace endstone_worldgen {
[[nodiscard]] bool isSupportedBds2630Build(std::string_view build) noexcept;
[[nodiscard]] bool isExpectedBds2630Build(std::string_view runtime_build,
                                          std::string_view packaged_build) noexcept;
[[nodiscard]] bool isExpectedEndstoneVersion(std::string_view runtime_version,
                                             std::string_view packaged_version) noexcept;
[[nodiscard]] std::unique_ptr<IVanillaGenerationAdapter> makeBds2630WorldGenAdapter(endstone::Server &server);
}
