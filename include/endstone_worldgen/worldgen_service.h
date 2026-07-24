#pragma once
#include "endstone_worldgen/generation_interceptor.h"
#include <endstone/plugin/service.h>
#include <memory>
#include <string_view>

namespace endstone_worldgen {
inline constexpr std::string_view WorldGenServiceName = "endstone:worldgen";

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
};

class WorldGenServiceProvider final : public WorldGenService {
public:
    WorldGenServiceProvider(IVanillaGenerationAdapter &adapter, GenerationInterceptor &interceptor)
        : adapter_(adapter), interceptor_(interceptor) {}
    void registerPopulator(std::shared_ptr<IPopulator> populator) override { interceptor_.addPopulator(std::move(populator)); }
    void clearPopulators() override { interceptor_.clearPopulators(); }
    [[nodiscard]] std::size_t populatorCount() const override { return interceptor_.populatorCount(); }
    [[nodiscard]] InterceptorStats stats() const override { return interceptor_.stats(); }
    [[nodiscard]] NativeDiagnostics diagnostics() const override { return adapter_.diagnostics(); }
    [[nodiscard]] bool interceptionActive() const override { return adapter_.diagnostics().interception_installed; }
private:
    IVanillaGenerationAdapter &adapter_;
    GenerationInterceptor &interceptor_;
};
}
