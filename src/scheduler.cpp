#include "scheduler.h"

#include "input_utils.h"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace osbasic {
namespace {

struct Process {
    int pid{};
    int arrival{};
    int burst{};
    int priority{};
};

struct Segment {
    int pid{};
    int start{};
    int end{};
};

struct Metrics {
    int pid{};
    int arrival{};
    int burst{};
    int priority{};
    int firstStart{-1};
    int completion{};
    int turnaround{};
    int waiting{};
    int response{};
    double weightedTurnaround{};
};

using ScheduleResult = std::pair<std::vector<Segment>, std::vector<Metrics>>;

int readInt(const std::string& prompt, int minValue) {
    while (true) {
        std::cout << prompt;
        int value = 0;
        if (readIntegerToken(value) && value >= minValue) {
            return value;
        }
        if (std::cin.eof()) {
            exitOnInputEnd();
        }
        std::cout << "输入不合法，请输入不小于 " << minValue << " 的整数。\n";
        std::cin.clear();
    }
}

std::vector<Process> inputProcesses() {
    const int n = readInt("请输入进程数: ", 1);
    std::vector<Process> processes;
    processes.reserve(n);
    for (int i = 0; i < n; ++i) {
        Process p;
        p.pid = i + 1;
        std::cout << "P" << p.pid << ":\n";
        p.arrival = readInt("  到达时间: ", 0);
        p.burst = readInt("  服务时间: ", 1);
        p.priority = readInt("  优先级(数值越小优先级越高): ", 0);
        processes.push_back(p);
    }
    return processes;
}

void addSegment(std::vector<Segment>& timeline, int pid, int start, int end) {
    if (start == end) return;
    if (!timeline.empty() && timeline.back().pid == pid && timeline.back().end == start) {
        timeline.back().end = end;
        return;
    }
    timeline.push_back({pid, start, end});
}

std::vector<Metrics> buildMetrics(const std::vector<Process>& processes,
                                  const std::vector<int>& completion,
                                  const std::vector<int>& firstStart) {
    std::vector<Metrics> rows;
    rows.reserve(processes.size());
    for (const auto& p : processes) {
        const int finish = completion[p.pid];
        const int start = firstStart[p.pid] == -1 ? p.arrival : firstStart[p.pid];
        const int turnaround = finish - p.arrival;
        rows.push_back({p.pid, p.arrival, p.burst, p.priority, start, finish, turnaround,
                        turnaround - p.burst, start - p.arrival,
                        static_cast<double>(turnaround) / p.burst});
    }
    std::sort(rows.begin(), rows.end(), [](const Metrics& a, const Metrics& b) {
        return a.pid < b.pid;
    });
    return rows;
}

ScheduleResult fcfs(std::vector<Process> processes) {
    std::sort(processes.begin(), processes.end(), [](const Process& a, const Process& b) {
        if (a.arrival != b.arrival) return a.arrival < b.arrival;
        return a.pid < b.pid;
    });

    std::vector<Segment> timeline;
    std::vector<int> completion(processes.size() + 1, 0);
    std::vector<int> firstStart(processes.size() + 1, -1);
    int now = 0;
    for (const auto& p : processes) {
        if (now < p.arrival) {
            addSegment(timeline, -1, now, p.arrival);
            now = p.arrival;
        }
        firstStart[p.pid] = now;
        addSegment(timeline, p.pid, now, now + p.burst);
        now += p.burst;
        completion[p.pid] = now;
    }
    return {timeline, buildMetrics(processes, completion, firstStart)};
}

ScheduleResult sjf(std::vector<Process> processes) {
    const int n = static_cast<int>(processes.size());
    std::vector<bool> done(n, false);
    std::vector<int> completion(n + 1, 0), firstStart(n + 1, -1);
    std::vector<Segment> timeline;
    int finished = 0, now = 0;

    while (finished < n) {
        int chosen = -1;
        for (int i = 0; i < n; ++i) {
            if (done[i] || processes[i].arrival > now) continue;
            if (chosen == -1 || processes[i].burst < processes[chosen].burst ||
                (processes[i].burst == processes[chosen].burst &&
                 processes[i].arrival < processes[chosen].arrival)) {
                chosen = i;
            }
        }
        if (chosen == -1) {
            int nextArrival = std::numeric_limits<int>::max();
            for (int i = 0; i < n; ++i) {
                if (!done[i]) nextArrival = std::min(nextArrival, processes[i].arrival);
            }
            addSegment(timeline, -1, now, nextArrival);
            now = nextArrival;
            continue;
        }
        const auto& p = processes[chosen];
        firstStart[p.pid] = now;
        addSegment(timeline, p.pid, now, now + p.burst);
        now += p.burst;
        completion[p.pid] = now;
        done[chosen] = true;
        ++finished;
    }
    return {timeline, buildMetrics(processes, completion, firstStart)};
}

ScheduleResult prioritySchedule(std::vector<Process> processes) {
    const int n = static_cast<int>(processes.size());
    std::vector<bool> done(n, false);
    std::vector<int> completion(n + 1, 0), firstStart(n + 1, -1);
    std::vector<Segment> timeline;
    int finished = 0, now = 0;

    while (finished < n) {
        int chosen = -1;
        for (int i = 0; i < n; ++i) {
            if (done[i] || processes[i].arrival > now) continue;
            if (chosen == -1 || processes[i].priority < processes[chosen].priority ||
                (processes[i].priority == processes[chosen].priority &&
                 processes[i].arrival < processes[chosen].arrival)) {
                chosen = i;
            }
        }
        if (chosen == -1) {
            int nextArrival = std::numeric_limits<int>::max();
            for (int i = 0; i < n; ++i) {
                if (!done[i]) nextArrival = std::min(nextArrival, processes[i].arrival);
            }
            addSegment(timeline, -1, now, nextArrival);
            now = nextArrival;
            continue;
        }
        const auto& p = processes[chosen];
        firstStart[p.pid] = now;
        addSegment(timeline, p.pid, now, now + p.burst);
        now += p.burst;
        completion[p.pid] = now;
        done[chosen] = true;
        ++finished;
    }
    return {timeline, buildMetrics(processes, completion, firstStart)};
}

ScheduleResult roundRobin(std::vector<Process> processes, int quantum) {
    std::sort(processes.begin(), processes.end(), [](const Process& a, const Process& b) {
        if (a.arrival != b.arrival) return a.arrival < b.arrival;
        return a.pid < b.pid;
    });

    const int n = static_cast<int>(processes.size());
    std::vector<int> remaining(n + 1, 0), completion(n + 1, 0), firstStart(n + 1, -1);
    for (const auto& p : processes) remaining[p.pid] = p.burst;

    std::deque<int> ready;
    std::vector<Segment> timeline;
    int now = 0, next = 0, finished = 0;
    while (finished < n) {
        while (next < n && processes[next].arrival <= now) ready.push_back(next++);
        if (ready.empty()) {
            const int nextArrival = processes[next].arrival;
            addSegment(timeline, -1, now, nextArrival);
            now = nextArrival;
            continue;
        }
        const int idx = ready.front();
        ready.pop_front();
        Process& p = processes[idx];
        if (firstStart[p.pid] == -1) firstStart[p.pid] = now;
        const int runTime = std::min(quantum, remaining[p.pid]);
        addSegment(timeline, p.pid, now, now + runTime);
        now += runTime;
        remaining[p.pid] -= runTime;
        while (next < n && processes[next].arrival <= now) ready.push_back(next++);
        if (remaining[p.pid] > 0) {
            ready.push_back(idx);
        } else {
            completion[p.pid] = now;
            ++finished;
        }
    }
    return {timeline, buildMetrics(processes, completion, firstStart)};
}

ScheduleResult srtf(std::vector<Process> processes) {
    const int n = static_cast<int>(processes.size());
    std::vector<int> remaining(n + 1, 0), completion(n + 1, 0), firstStart(n + 1, -1);
    for (const auto& p : processes) remaining[p.pid] = p.burst;
    std::vector<Segment> timeline;
    int now = 0, finished = 0;

    while (finished < n) {
        int chosen = -1;
        for (int i = 0; i < n; ++i) {
            const Process& p = processes[i];
            if (p.arrival > now || remaining[p.pid] == 0) continue;
            if (chosen == -1 || remaining[p.pid] < remaining[processes[chosen].pid] ||
                (remaining[p.pid] == remaining[processes[chosen].pid] &&
                 p.arrival < processes[chosen].arrival)) {
                chosen = i;
            }
        }
        if (chosen == -1) {
            int nextArrival = std::numeric_limits<int>::max();
            for (const auto& p : processes) {
                if (remaining[p.pid] > 0) nextArrival = std::min(nextArrival, p.arrival);
            }
            addSegment(timeline, -1, now, nextArrival);
            now = nextArrival;
            continue;
        }
        Process& p = processes[chosen];
        if (firstStart[p.pid] == -1) firstStart[p.pid] = now;
        addSegment(timeline, p.pid, now, now + 1);
        --remaining[p.pid];
        ++now;
        if (remaining[p.pid] == 0) {
            completion[p.pid] = now;
            ++finished;
        }
    }
    return {timeline, buildMetrics(processes, completion, firstStart)};
}

ScheduleResult preemptivePriority(std::vector<Process> processes) {
    const int n = static_cast<int>(processes.size());
    std::vector<int> remaining(n + 1, 0), completion(n + 1, 0), firstStart(n + 1, -1);
    for (const auto& p : processes) remaining[p.pid] = p.burst;
    std::vector<Segment> timeline;
    int now = 0, finished = 0;

    while (finished < n) {
        int chosen = -1;
        for (int i = 0; i < n; ++i) {
            const Process& p = processes[i];
            if (p.arrival > now || remaining[p.pid] == 0) continue;
            if (chosen == -1 || p.priority < processes[chosen].priority ||
                (p.priority == processes[chosen].priority &&
                 p.arrival < processes[chosen].arrival)) {
                chosen = i;
            }
        }
        if (chosen == -1) {
            int nextArrival = std::numeric_limits<int>::max();
            for (const auto& p : processes) {
                if (remaining[p.pid] > 0) nextArrival = std::min(nextArrival, p.arrival);
            }
            addSegment(timeline, -1, now, nextArrival);
            now = nextArrival;
            continue;
        }
        Process& p = processes[chosen];
        if (firstStart[p.pid] == -1) firstStart[p.pid] = now;
        addSegment(timeline, p.pid, now, now + 1);
        --remaining[p.pid];
        ++now;
        if (remaining[p.pid] == 0) {
            completion[p.pid] = now;
            ++finished;
        }
    }
    return {timeline, buildMetrics(processes, completion, firstStart)};
}

void printResult(const std::string& name, const ScheduleResult& result) {
    const auto& timeline = result.first;
    const auto& metrics = result.second;
    std::cout << "\n--- " << name << " 调度结果 ---\n";
    std::cout << "运行顺序(Gantt): ";
    for (const auto& s : timeline) {
        std::cout << "[" << s.start << "," << s.end << ":"
                  << (s.pid == -1 ? "IDLE" : "P" + std::to_string(s.pid)) << "] ";
    }
    std::cout << "\n\n";

    double totalTurnaround = 0, totalWaiting = 0, totalWeighted = 0, totalResponse = 0;
    int busyTime = 0;
    for (const auto& row : metrics) busyTime += row.burst;
    const int firstTime = timeline.empty() ? 0 : timeline.front().start;
    const int lastTime = timeline.empty() ? 0 : timeline.back().end;
    const int totalTime = std::max(1, lastTime - firstTime);

    std::cout << std::left << std::setw(8) << "进程" << std::setw(8) << "到达"
              << std::setw(8) << "服务" << std::setw(8) << "优先级"
              << std::setw(8) << "开始" << std::setw(8) << "完成"
              << std::setw(8) << "周转" << std::setw(8) << "等待"
              << std::setw(8) << "响应" << std::setw(10) << "带权周转" << "\n";
    for (const auto& row : metrics) {
        totalTurnaround += row.turnaround;
        totalWaiting += row.waiting;
        totalWeighted += row.weightedTurnaround;
        totalResponse += row.response;
        std::cout << std::left << std::setw(8) << ("P" + std::to_string(row.pid))
                  << std::setw(8) << row.arrival << std::setw(8) << row.burst
                  << std::setw(8) << row.priority << std::setw(8) << row.firstStart
                  << std::setw(8) << row.completion << std::setw(8) << row.turnaround
                  << std::setw(8) << row.waiting << std::setw(8) << row.response
                  << std::setw(10) << std::fixed << std::setprecision(2)
                  << row.weightedTurnaround << "\n";
    }
    const double n = static_cast<double>(metrics.size());
    std::cout << std::fixed << std::setprecision(2)
              << "平均周转时间: " << totalTurnaround / n
              << "，平均等待时间: " << totalWaiting / n
              << "，平均响应时间: " << totalResponse / n
              << "，平均带权周转时间: " << totalWeighted / n << "\n"
              << "CPU利用率: " << (100.0 * busyTime / totalTime)
              << "%，吞吐量: " << (n / totalTime) << " 进程/时间单位\n";
}

void printAnalysis() {
    std::cout << "\n算法特点分析:\n"
              << "FCFS 简单公平，但短作业可能被长作业阻塞，存在护航效应。\n"
              << "SJF/SRTF 倾向降低平均等待时间；SRTF 是抢占式版本，响应更快，但需要维护剩余时间。\n"
              << "RR 适合分时系统，公平性好；时间片过小会增加切换开销，过大则趋近 FCFS。\n"
              << "优先级调度能体现任务紧急程度；抢占式优先级响应更及时，但低优先级进程可能饥饿。\n";
}

std::vector<Process> demoProcesses() {
    return {{1, 0, 8, 3}, {2, 1, 4, 1}, {3, 2, 9, 4},
            {4, 3, 5, 2}, {5, 6, 2, 1}, {6, 8, 6, 3}};
}

}  // namespace

void runSchedulerDemo() {
    auto data = demoProcesses();
    std::cout << "\n========== 处理机调度演示 ==========\n";
    printResult("FCFS", fcfs(data));
    printResult("SJF", sjf(data));
    printResult("SRTF", srtf(data));
    printResult("RR(q=3)", roundRobin(data, 3));
    printResult("Priority", prioritySchedule(data));
    printResult("PriorityPreemptive", preemptivePriority(data));
    printAnalysis();
}

void runSchedulerMenu() {
    std::vector<Process> processes = inputProcesses();
    while (true) {
        std::cout << "\n处理机调度算法:\n"
                  << "1. FCFS 先来先服务\n"
                  << "2. SJF 短作业优先(非抢占)\n"
                  << "3. RR 时间片轮转\n"
                  << "4. Priority 优先级调度(非抢占)\n"
                  << "5. SRTF 最短剩余时间优先(抢占)\n"
                  << "6. Priority 抢占式优先级调度\n"
                  << "7. 六种算法全部比较\n"
                  << "0. 返回主菜单\n";
        const int choice = readInt("请选择: ", 0);
        if (choice == 0) return;
        int q = 0;
        if (choice == 3 || choice == 7) q = readInt("请输入时间片 q: ", 1);
        if (choice == 1 || choice == 7) printResult("FCFS", fcfs(processes));
        if (choice == 2 || choice == 7) printResult("SJF", sjf(processes));
        if (choice == 3 || choice == 7) printResult("RR", roundRobin(processes, q));
        if (choice == 4 || choice == 7) printResult("Priority", prioritySchedule(processes));
        if (choice == 5 || choice == 7) printResult("SRTF", srtf(processes));
        if (choice == 6 || choice == 7) {
            printResult("PriorityPreemptive", preemptivePriority(processes));
        }
        if (choice >= 1 && choice <= 7) {
            printAnalysis();
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

}  // namespace osbasic
