#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include <string>

namespace refix::source {

class MainThreadQueue {
public:
    static void Enqueue(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(GetMutex());
        GetTasks().push_back(std::move(task));
    }

    static void ExecuteClientCmd(const std::string& cmd) {
        Enqueue([cmd]() {
            ExecuteClientCmdImmediate(cmd.c_str());
        });
    }

    static void Drain() {
        std::vector<std::function<void()>> toRun;
        {
            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetTasks().empty()) return;
            toRun.swap(GetTasks());
        }
        for (auto& task : toRun) {
            if (task) {
                task();
            }
        }
    }

    static void ExecuteClientCmdImmediate(const char* cmd);

private:
    static std::mutex& GetMutex() {
        static std::mutex s_mtx;
        return s_mtx;
    }
    static std::vector<std::function<void()>>& GetTasks() {
        static std::vector<std::function<void()>> s_tasks;
        return s_tasks;
    }
};

std::string GetConVarString(const char* name);

} // namespace refix::source
