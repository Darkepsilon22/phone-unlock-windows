#include "support/worker_group.h"

WorkerGroup::~WorkerGroup()
{
    JoinAll();
}

void WorkerGroup::Start(std::function<void()> work)
{
    JoinFinished();
    auto finished = std::make_shared<std::atomic<bool>>(false);
    workers_.push_back({ std::thread([work = std::move(work), finished] {
        work();
        finished->store(true);
    }), finished });
}

size_t WorkerGroup::ActiveCount()
{
    JoinFinished();
    return workers_.size();
}

void WorkerGroup::JoinAll()
{
    for (Worker& worker : workers_)
    {
        worker.thread.join();
    }
    workers_.clear();
}

void WorkerGroup::JoinFinished()
{
    workers_.remove_if([](Worker& worker) {
        if (!worker.finished->load())
        {
            return false;
        }
        worker.thread.join();
        return true;
    });
}
