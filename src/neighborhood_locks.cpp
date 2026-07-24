#include "endstone_worldgen/neighborhood_locks.h"
#include <algorithm>
namespace endstone_worldgen {
NeighborhoodLockManager::Guard::~Guard(){if(m_)m_->release(p_);}
NeighborhoodLockManager::Guard NeighborhoodLockManager::acquire(ChunkPos c,int r){std::vector<ChunkPos>p;for(int x=c.x-r;x<=c.x+r;++x)for(int z=c.z-r;z<=c.z+r;++z)p.push_back({x,z});std::sort(p.begin(),p.end(),[](auto&a,auto&b){return a.x==b.x?a.z<b.z:a.x<b.x;});std::unique_lock lock(mu_);cv_.wait(lock,[&]{for(auto q:p)if(locked_.contains({q.x,q.z}))return false;return true;});for(auto q:p)locked_.insert({q.x,q.z});return Guard(this,std::move(p));}
void NeighborhoodLockManager::release(const std::vector<ChunkPos>&p){{std::lock_guard lock(mu_);for(auto q:p)locked_.erase({q.x,q.z});}cv_.notify_all();}
}
