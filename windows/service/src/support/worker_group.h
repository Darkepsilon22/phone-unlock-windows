#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <thread>

class WorkerGroup
{
public:
    WorkerGroup() = default;
    WorkerGroup(const WorkerGroup&) = delete;
    WorkerGroup& operator=(const WorkerGroup&) = delete;
    ~WorkerGroup();

    void Start(std::function<void()> work);
    size_t ActiveCount();
    void JoinAll();

private:
    struct Worker
    {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    void JoinFinished();

    std::list<Worker> workers_;
};
