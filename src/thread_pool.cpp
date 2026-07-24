#include "endstone_worldgen/thread_pool.h"
namespace endstone_worldgen {
ThreadPool::ThreadPool(size_t count){count=std::max<size_t>(1,count);for(size_t i=0;i<count;++i)workers_.emplace_back([this]{for(;;){Job j;{std::unique_lock lock(mu_);cv_.wait(lock,[&]{return stopping_||!jobs_.empty();});if(stopping_&&jobs_.empty())return;j=jobs_.top();jobs_.pop();}j.fn();}});}
ThreadPool::~ThreadPool(){{std::lock_guard lock(mu_);stopping_=true;}cv_.notify_all();for(auto&t:workers_)if(t.joinable())t.join();}
}
