#pragma once
/** @file ParallelExecutor.h
 * @brief Persistent CPU worker pool for independent indexed preparation jobs.
 */

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

/** @brief Reuses worker threads for CPU-only, independent jobs.
 * @warning Jobs must not throw and must be safe to execute in arbitrary index
 * order and concurrently with the caller. SDL rendering and game lifecycle
 * callbacks must remain on the main thread. `Run` is not reentrant and cannot
 * be called by simultaneous dispatchers.
 */
class ParallelExecutor final
{
public:
    /** @brief Starts the requested number of persistent worker threads.
     * @param workers Worker threads in addition to the calling thread, which
     *                also processes jobs during Run().
     */
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

    /** @brief Number of background threads, excluding the caller.
     */
    unsigned WorkerCount() const noexcept { return static_cast<unsigned>(threads_.size()); }
    /** @brief Time the caller spent waiting after processing its own jobs, in milliseconds.
     * @note This is **not** total dispatch or execution time.
     */
    float LastWaitMs() const noexcept { return lastWaitMs_; }

    /** @brief Invokes task(i) once for each index in [0, count) and waits for all jobs.
     * @param count Total independent jobs; count < 2 runs on the caller only.
     * @param task Callback valid for the duration of Run; must not throw or
     *             access shared mutable state without synchronization.
     * @warning Never call concurrently, recursively or from one of the jobs.
     */
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
