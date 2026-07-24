#pragma once
#include "endstone_worldgen/generator.h"
#include "endstone_worldgen/neighborhood_locks.h"
#include "endstone_worldgen/thread_pool.h"
#include <atomic>
#include <future>
#include <memory>
namespace endstone_worldgen {
struct GenerationResult { ChunkBuffer chunk; std::string generator; std::uint64_t fingerprint{}; };
class GenerationScheduler {
public:
 explicit GenerationScheduler(size_t workers):pool_(workers){}
 std::future<GenerationResult> generate(GenerationContext context,std::shared_ptr<IGenerator> generator,JobPriority priority=JobPriority::Background);
 std::future<GenerationResult> populate(GenerationContext context,ChunkBuffer chunk,std::shared_ptr<IPopulator> populator,JobPriority priority=JobPriority::Background);
 std::future<GenerationResult> populatePipeline(GenerationContext context,ChunkBuffer chunk,
     std::vector<std::shared_ptr<IPopulator>> populators,JobPriority priority=JobPriority::Background);
 [[nodiscard]] size_t workerCount()const noexcept{return pool_.workerCount();}
private:ThreadPool pool_;NeighborhoodLockManager locks_;
};
}
