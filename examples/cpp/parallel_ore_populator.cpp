// Example consumer plugin fragment. It registers a worker-safe populator with
// the live WorldGen service. No Endstone/BDS object may be retained by IPopulator.
#include <endstone/endstone.hpp>
#include "endstone_worldgen/worldgen_service.h"
#include <memory>
#include <random>

using namespace endstone_worldgen;

class ExampleOrePopulator final : public IPopulator {
public:
    [[nodiscard]] std::string_view identifier() const noexcept override { return "example:ore"; }
    [[nodiscard]] int radius() const noexcept override { return 1; }

    void populate(const GenerationContext &context, ChunkBuffer &chunk) override {
        std::mt19937_64 random(context.stage_seed);
        for (int vein = 0; vein < 4; ++vein) {
            const int x = static_cast<int>(random() % 16);
            const int z = static_cast<int>(random() % 16);
            const int y = chunk.minY() + 16 + static_cast<int>(random() % 32);
            chunk.setRuntimeId(x, y, z, 0); // Replace 0 with a registered palette runtime ID.
        }
    }
};

void registerWithWorldGen(endstone::Server &server) {
    auto api = server.getServiceManager().load<WorldGenService>(std::string(WorldGenServiceName));
    if (api) api->registerPopulator(std::make_shared<ExampleOrePopulator>());
}
