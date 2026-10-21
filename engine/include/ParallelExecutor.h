#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// Persistent CPU workers for independent, non-mutating jobs. SDL and game
// lifecycle calls must remain on the main thread. Jobs must not throw.
class ParallelExecutor final
{
public:
    explicit ParallelExecutor(unsigned workers)
    {
        threads_.reserve(workers);
        try
        {
            for (unsigned i = 0; i < workers; ++i)
                threads_.emplace_back([this] { WorkerLoop_(); });
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stopping_ = true;
                ++generation_;
            }
            ready_.notify_all();
            for (auto& thread : threads_)
                if (thread.joinable()) thread.join();
            throw;
        }
    }

    ~ParallelExecutor()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            ++generation_;
        }
        ready_.notify_all();
        for (auto& thread : threads_)
            if (thread.joinable()) thread.join();
    }

    ParallelExecutor(const ParallelExecutor&) = delete;
    ParallelExecutor& operator=(const ParallelExecutor&) = delete;

    unsigned WorkerCount() const noexcept { return static_cast<unsigned>(threads_.size()); }
    float LastWaitMs() const noexcept { return lastWaitMs_; }

    // The caller also processes jobs, so short workloads can stay on one thread.
    // This function must not be called concurrently from multiple dispatchers.
    void Run(std::size_t count, const std::function<void(std::size_t)>& task)
    {
        lastWaitMs_ = 0.f;
        if (threads_.empty() || count < 2)
        {
            for (std::size_t i = 0; i < count; ++i) task(i);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            task_ = &task;
            count_ = count;
            next_.store(0, std::memory_order_relaxed);
            remaining_ = threads_.size();
            ++generation_;
        }
        ready_.notify_all();
        Process_(task, count);
        const auto waitStart = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> lock(mutex_);
        finished_.wait(lock, [this] { return remaining_ == 0; });
        lastWaitMs_ = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - waitStart).count();
        task_ = nullptr;
    }

private:
    void Process_(const std::function<void(std::size_t)>& task, std::size_t count)
    {
        for (;;)
        {
            const auto index = next_.fetch_add(1, std::memory_order_relaxed);
            if (index >= count) break;
            task(index);
        }
    }

    void WorkerLoop_()
    {
        std::uint64_t observedGeneration = 0;
        for (;;)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this, &observedGeneration] {
                return stopping_ || generation_ != observedGeneration;
            });
            if (stopping_) return;
            observedGeneration = generation_;
            const auto* task = task_;
            const auto count = count_;
            lock.unlock();

            if (task) Process_(*task, count);

            lock.lock();
            if (--remaining_ == 0) finished_.notify_one();
            lock.unlock(); // Release before the next iteration acquires mutex_.
        }
    }

    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::condition_variable finished_;
    std::atomic<std::size_t> next_{0};
    const std::function<void(std::size_t)>* task_ = nullptr;
    std::size_t count_ = 0;
    std::size_t remaining_ = 0;
    std::uint64_t generation_ = 0;
    bool stopping_ = false;
    float lastWaitMs_ = 0.f;
};
