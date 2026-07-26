#include "endstone_worldgen/neighborhood_locks.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace endstone_worldgen {
NeighborhoodLockManager::Guard::~Guard(){if(m_)m_->release(p_);}
NeighborhoodLockManager::Guard& NeighborhoodLockManager::Guard::operator=(Guard&&other)noexcept{if(this!=&other){if(m_)m_->release(p_);m_=other.m_;p_=std::move(other.p_);other.m_=nullptr;}return *this;}
NeighborhoodLockManager::Guard NeighborhoodLockManager::acquire(ChunkPos c,int r){
    if(r<0)throw std::invalid_argument("neighborhood radius must not be negative");
    const auto min_x=static_cast<std::int64_t>(c.x)-r;
    const auto max_x=static_cast<std::int64_t>(c.x)+r;
    const auto min_z=static_cast<std::int64_t>(c.z)-r;
    const auto max_z=static_cast<std::int64_t>(c.z)+r;
    if(min_x<std::numeric_limits<std::int32_t>::min()||max_x>std::numeric_limits<std::int32_t>::max()||
       min_z<std::numeric_limits<std::int32_t>::min()||max_z>std::numeric_limits<std::int32_t>::max())
        throw std::out_of_range("neighborhood exceeds chunk coordinate range");
    const auto diameter=static_cast<std::uint64_t>(r)*2+1;
    if(diameter>std::numeric_limits<std::size_t>::max()/diameter)
        throw std::length_error("neighborhood is too large");
    std::vector<ChunkPos>p;
    const auto count=static_cast<std::size_t>(diameter*diameter);
    if(count>p.max_size())throw std::length_error("neighborhood is too large");
    p.reserve(count);
    for(auto x=min_x;x<=max_x;++x)for(auto z=min_z;z<=max_z;++z)
        p.push_back({static_cast<std::int32_t>(x),static_cast<std::int32_t>(z)});
    std::sort(p.begin(),p.end(),[](auto&a,auto&b){return a.x==b.x?a.z<b.z:a.x<b.x;});
    std::unique_lock lock(mu_);
    cv_.wait(lock,[&]{for(auto q:p)if(locked_.contains({q.x,q.z}))return false;return true;});
    for(auto q:p)locked_.insert({q.x,q.z});
    return Guard(this,std::move(p));
}
void NeighborhoodLockManager::release(const std::vector<ChunkPos>&p){{std::lock_guard lock(mu_);for(auto q:p)locked_.erase({q.x,q.z});}cv_.notify_all();}
}
