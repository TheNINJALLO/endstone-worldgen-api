#pragma once
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
namespace endstone_worldgen {
class ThreadPool {
public:
    explicit ThreadPool(size_t workers); ~ThreadPool(); ThreadPool(const ThreadPool&)=delete; ThreadPool& operator=(const ThreadPool&)=delete;
    template<class F> auto submit(int priority,F&& f)->std::future<std::invoke_result_t<F>> {
        using R=std::invoke_result_t<F>; auto task=std::make_shared<std::packaged_task<R()>>(std::forward<F>(f)); auto future=task->get_future();
        { std::lock_guard lock(mu_); if(stopping_) throw std::runtime_error("thread pool stopped"); jobs_.push(Job{priority,sequence_++,[task]{(*task)();}}); }
        cv_.notify_one(); return future;
    }
    [[nodiscard]] size_t workerCount() const noexcept{return workers_.size();}
private:
    struct Job{int priority;std::uint64_t sequence;std::function<void()> fn;};
    struct Compare{bool operator()(const Job&a,const Job&b)const{return a.priority==b.priority?a.sequence>b.sequence:a.priority<b.priority;}};
    std::mutex mu_;std::condition_variable cv_;std::priority_queue<Job,std::vector<Job>,Compare> jobs_;std::vector<std::thread> workers_;bool stopping_{};std::uint64_t sequence_{};
};
}
