#pragma once
#include "endstone_worldgen/types.h"
#include <condition_variable>
#include <mutex>
#include <set>
#include <vector>
namespace endstone_worldgen {
class NeighborhoodLockManager {
public:
 class Guard{public:Guard()=default;Guard(NeighborhoodLockManager*m,std::vector<ChunkPos>p):m_(m),p_(std::move(p)){} Guard(Guard&&o)noexcept:m_(o.m_),p_(std::move(o.p_)){o.m_=nullptr;} ~Guard();private:NeighborhoodLockManager*m_{};std::vector<ChunkPos>p_;};
 Guard acquire(ChunkPos center,int radius);
private:void release(const std::vector<ChunkPos>&);std::mutex mu_;std::condition_variable cv_;std::set<std::pair<int,int>> locked_;friend class Guard;
};
}
