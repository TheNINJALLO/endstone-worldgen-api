#include "endstone_worldgen/generation_scheduler.h"
#include <algorithm>
namespace endstone_worldgen {
std::future<GenerationResult> GenerationScheduler::generate(GenerationContext c,std::shared_ptr<IGenerator> g,JobPriority p){return pool_.submit(int(p),[c=std::move(c),g=std::move(g)]()mutable{c.stage_seed=deterministicStageSeed(c.world_seed,c.dimension,c.chunk,c.stage);ChunkBuffer chunk(c.chunk);g->generate(c,chunk);auto fp=chunk.fingerprint();return GenerationResult{std::move(chunk),std::string(g->identifier()),fp};});}
std::future<GenerationResult> GenerationScheduler::populate(GenerationContext c,ChunkBuffer chunk,std::shared_ptr<IPopulator> pop,JobPriority p){return pool_.submit(int(p),[this,c=std::move(c),chunk=std::move(chunk),pop=std::move(pop)]()mutable{auto guard=locks_.acquire(c.chunk,pop->radius());c.stage_seed=deterministicStageSeed(c.world_seed,c.dimension,c.chunk,c.stage);pop->populate(c,chunk);auto fp=chunk.fingerprint();return GenerationResult{std::move(chunk),std::string(pop->identifier()),fp};});}
std::future<GenerationResult> GenerationScheduler::populatePipeline(GenerationContext c,ChunkBuffer chunk,
    std::vector<std::shared_ptr<IPopulator>> populators,JobPriority p){
    return pool_.submit(int(p),[this,c=std::move(c),chunk=std::move(chunk),populators=std::move(populators)]()mutable{
        int radius=0; for(const auto &pop:populators) if(pop) radius=std::max(radius,pop->radius());
        auto guard=locks_.acquire(c.chunk,radius); std::string names;
        for(std::size_t i=0;i<populators.size();++i){auto &pop=populators[i];if(!pop)continue;
            c.stage=GenerationStage::Decoration;c.stage_seed=deterministicStageSeed(c.world_seed,c.dimension,c.chunk,c.stage)^std::hash<std::string_view>{}(pop->identifier());
            pop->populate(c,chunk);if(!names.empty())names+=",";names+=pop->identifier();}
        auto fp=chunk.fingerprint();return GenerationResult{std::move(chunk),std::move(names),fp};
    });
}
}
