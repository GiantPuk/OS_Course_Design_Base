#include "memory.h"

#include "input_utils.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace osbasic {
namespace {

struct Partition {
    int start{};
    int size{};
    bool free{true};
    std::string owner{"FREE"};
};

int readInt(const std::string& prompt, int minValue) {
    while (true) {
        std::cout << prompt;
        int value = 0;
        if (readIntegerToken(value) && value >= minValue) return value;
        if (std::cin.eof()) {
            exitOnInputEnd();
        }
        std::cout << "输入不合法，请输入不小于 " << minValue << " 的整数。\n";
        std::cin.clear();
    }
}

std::string readName(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    std::cin >> value;
    return value;
}

class PartitionManager {
public:
    explicit PartitionManager(int totalSize)
        : totalSize_(totalSize), partitions_{{0, totalSize, true, "FREE"}} {}

    bool allocateFirstFit(const std::string& name, int size) {
        for (std::size_t i = 0; i < partitions_.size(); ++i) {
            if (canUse(i, size)) {
                allocateAt(i, name, size);
                return true;
            }
        }
        return false;
    }

    bool allocateBestFit(const std::string& name, int size) {
        int best = -1;
        for (std::size_t i = 0; i < partitions_.size(); ++i) {
            if (!canUse(i, size)) continue;
            if (best == -1 || partitions_[i].size < partitions_[best].size) {
                best = static_cast<int>(i);
            }
        }
        if (best == -1) return false;
        allocateAt(static_cast<std::size_t>(best), name, size);
        return true;
    }

    bool allocateWorstFit(const std::string& name, int size) {
        int worst = -1;
        for (std::size_t i = 0; i < partitions_.size(); ++i) {
            if (!canUse(i, size)) continue;
            if (worst == -1 || partitions_[i].size > partitions_[worst].size) {
                worst = static_cast<int>(i);
            }
        }
        if (worst == -1) return false;
        allocateAt(static_cast<std::size_t>(worst), name, size);
        return true;
    }

    bool release(const std::string& name) {
        for (auto& block : partitions_) {
            if (!block.free && block.owner == name) {
                block.free = true;
                block.owner = "FREE";
                coalesce();
                return true;
            }
        }
        return false;
    }

    void print() const {
        std::cout << "\n当前动态分区表:\n";
        std::cout << std::left << std::setw(10) << "起址" << std::setw(10) << "大小"
                  << std::setw(10) << "状态" << std::setw(12) << "作业" << "\n";
        for (const auto& block : partitions_) {
            std::cout << std::left << std::setw(10) << block.start << std::setw(10)
                      << block.size << std::setw(10) << (block.free ? "空闲" : "占用")
                      << std::setw(12) << block.owner << "\n";
        }
        printStats();
    }

    void printStats() const {
        int used = 0, free = 0, freeBlocks = 0, largestFree = 0;
        for (const auto& block : partitions_) {
            if (block.free) {
                free += block.size;
                ++freeBlocks;
                largestFree = std::max(largestFree, block.size);
            } else {
                used += block.size;
            }
        }
        std::cout << std::fixed << std::setprecision(2)
                  << "分区数: " << partitions_.size()
                  << "，外部碎片数: " << freeBlocks
                  << "，最大空闲块: " << largestFree
                  << "，总空闲: " << free
                  << "，内存利用率: " << (100.0 * used / totalSize_) << "%\n";
    }

private:
    bool canUse(std::size_t index, int size) const {
        return partitions_[index].free && partitions_[index].size >= size;
    }

    void allocateAt(std::size_t index, const std::string& name, int size) {
        Partition& block = partitions_[index];
        const int remain = block.size - size;
        block.size = size;
        block.free = false;
        block.owner = name;
        if (remain > 0) {
            partitions_.insert(partitions_.begin() + static_cast<long long>(index) + 1,
                               {block.start + size, remain, true, "FREE"});
        }
    }

    void coalesce() {
        for (std::size_t i = 0; i + 1 < partitions_.size();) {
            if (partitions_[i].free && partitions_[i + 1].free) {
                partitions_[i].size += partitions_[i + 1].size;
                partitions_.erase(partitions_.begin() + static_cast<long long>(i) + 1);
            } else {
                ++i;
            }
        }
    }

    int totalSize_;
    std::vector<Partition> partitions_;
};

struct Request {
    char op;
    std::string name;
    int size;
};

void applyRequest(PartitionManager& manager, const Request& req, int algorithm) {
    if (req.op == 'A') {
        bool ok = false;
        if (algorithm == 1) ok = manager.allocateFirstFit(req.name, req.size);
        if (algorithm == 2) ok = manager.allocateBestFit(req.name, req.size);
        if (algorithm == 3) ok = manager.allocateWorstFit(req.name, req.size);
        std::cout << "申请 " << req.name << "=" << req.size << ": "
                  << (ok ? "成功" : "失败") << "\n";
    } else {
        std::cout << "释放 " << req.name << ": "
                  << (manager.release(req.name) ? "成功" : "失败") << "\n";
    }
    manager.print();
}

void runPartitionDemo() {
    std::cout << "\n========== 动态分区管理演示 ==========\n";
    std::cout << "本组数据刻意构造多个可满足申请的空闲分区，用于体现 FF、BF、WF 的选择差异。\n";
    const std::vector<Request> seq{
        {'A', "A", 100}, {'A', "B", 200}, {'A', "C", 50}, {'A', "D", 80},
        {'A', "E", 70},  {'F', "B", 0},   {'F', "D", 0},  {'A', "F", 75},
        {'A', "G", 120}, {'A', "H", 60},
    };
    const std::vector<std::pair<int, std::string>> algs{{1, "FF 首次适应"},
                                                         {2, "BF 最佳适应"},
                                                         {3, "WF 最坏适应"}};
    for (const auto& [id, name] : algs) {
        std::cout << "\n--- " << name << " ---\n";
        PartitionManager manager(640);
        manager.print();
        for (const auto& req : seq) applyRequest(manager, req, id);
    }
    std::cout << "分析: FF 从低地址开始选择第一个可用空闲块，最终低地址区域被连续切分；"
              << "BF 优先使用最接近申请大小的空闲块，减少单次剩余但容易留下很小碎片；"
              << "WF 优先切分最大空闲块，使中小空闲块保留时间更长。"
              << "因此三种算法在本组数据下形成不同的最终分区表。\n";
}

void interactivePartition() {
    const int total = readInt("请输入内存总大小: ", 1);
    PartitionManager manager(total);
    while (true) {
        std::cout << "\n动态分区管理:\n"
                  << "1. 首次适应 FF 分配\n"
                  << "2. 最佳适应 BF 分配\n"
                  << "3. 最坏适应 WF 分配\n"
                  << "4. 回收分区\n"
                  << "5. 显示分区表与碎片统计\n"
                  << "0. 返回\n";
        const int choice = readInt("请选择: ", 0);
        if (choice == 0) return;
        if (choice >= 1 && choice <= 3) {
            const std::string name = readName("请输入作业名: ");
            const int size = readInt("请输入申请大小: ", 1);
            Request req{'A', name, size};
            applyRequest(manager, req, choice);
        } else if (choice == 4) {
            const std::string name = readName("请输入要回收的作业名: ");
            applyRequest(manager, {'F', name, 0}, 1);
        } else if (choice == 5) {
            manager.print();
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

struct PageStep {
    int page{};
    bool fault{};
    std::vector<int> frames;
};

int countFaults(const std::vector<PageStep>& steps) {
    return static_cast<int>(std::count_if(steps.begin(), steps.end(),
                                          [](const PageStep& s) { return s.fault; }));
}

void printPageResult(const std::string& name, const std::vector<PageStep>& steps,
                     int frameCount) {
    const int faults = countFaults(steps);
    const int hits = static_cast<int>(steps.size()) - faults;
    std::cout << "\n--- " << name << " 页面置换结果 ---\n";
    std::cout << std::left << std::setw(10) << "访问页" << std::setw(12) << "是否缺页"
              << "内存块状态\n";
    for (const auto& step : steps) {
        std::cout << std::left << std::setw(10) << step.page << std::setw(12)
                  << (step.fault ? "缺页" : "命中");
        for (int i = 0; i < frameCount; ++i) {
            if (i < static_cast<int>(step.frames.size())) {
                std::cout << step.frames[i] << " ";
            } else {
                std::cout << "- ";
            }
        }
        std::cout << "\n";
    }
    const double faultRate = steps.empty() ? 0.0 : static_cast<double>(faults) / steps.size();
    const double hitRate = steps.empty() ? 0.0 : static_cast<double>(hits) / steps.size();
    std::cout << "缺页次数: " << faults << "，访问次数: " << steps.size()
              << "，缺页率: " << std::fixed << std::setprecision(2) << faultRate * 100
              << "%，命中率: " << hitRate * 100 << "%\n";
}

std::vector<PageStep> fifoPages(const std::vector<int>& refs, int frameCount) {
    std::vector<int> frames;
    std::queue<int> order;
    std::vector<PageStep> steps;
    for (int page : refs) {
        const bool hit = std::find(frames.begin(), frames.end(), page) != frames.end();
        if (!hit) {
            if (static_cast<int>(frames.size()) < frameCount) {
                frames.push_back(page);
                order.push(page);
            } else {
                const int victim = order.front();
                order.pop();
                *std::find(frames.begin(), frames.end(), victim) = page;
                order.push(page);
            }
        }
        steps.push_back({page, !hit, frames});
    }
    return steps;
}

std::vector<PageStep> lruPages(const std::vector<int>& refs, int frameCount) {
    std::vector<int> frames;
    std::unordered_map<int, int> lastUse;
    std::vector<PageStep> steps;
    for (int time = 0; time < static_cast<int>(refs.size()); ++time) {
        const int page = refs[time];
        const bool hit = std::find(frames.begin(), frames.end(), page) != frames.end();
        if (!hit) {
            if (static_cast<int>(frames.size()) < frameCount) {
                frames.push_back(page);
            } else {
                auto victim = std::min_element(frames.begin(), frames.end(),
                    [&](int a, int b) { return lastUse[a] < lastUse[b]; });
                *victim = page;
            }
        }
        lastUse[page] = time;
        steps.push_back({page, !hit, frames});
    }
    return steps;
}

std::vector<PageStep> optPages(const std::vector<int>& refs, int frameCount) {
    std::vector<int> frames;
    std::vector<PageStep> steps;
    for (int i = 0; i < static_cast<int>(refs.size()); ++i) {
        const int page = refs[i];
        const bool hit = std::find(frames.begin(), frames.end(), page) != frames.end();
        if (!hit) {
            if (static_cast<int>(frames.size()) < frameCount) {
                frames.push_back(page);
            } else {
                int victimIndex = 0;
                int farthest = -1;
                for (int f = 0; f < static_cast<int>(frames.size()); ++f) {
                    int nextUse = std::numeric_limits<int>::max();
                    for (int j = i + 1; j < static_cast<int>(refs.size()); ++j) {
                        if (refs[j] == frames[f]) {
                            nextUse = j;
                            break;
                        }
                    }
                    if (nextUse > farthest) {
                        farthest = nextUse;
                        victimIndex = f;
                    }
                }
                frames[victimIndex] = page;
            }
        }
        steps.push_back({page, !hit, frames});
    }
    return steps;
}

std::vector<int> inputRefs() {
    const int n = readInt("请输入页面访问串长度: ", 1);
    std::vector<int> refs(n);
    std::cout << "请输入页面访问串(空格分隔): ";
    for (int& page : refs) {
        while (!readIntegerToken(page)) {
            if (std::cin.eof()) exitOnInputEnd();
            std::cout << "输入不合法，请输入整数: ";
            std::cin.clear();
        }
    }
    return refs;
}

void runBeladyExperiment() {
    std::cout << "\n========== Belady 异常验证 ==========\n";
    const std::vector<int> refs{1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    const int faults3 = countFaults(fifoPages(refs, 3));
    const int faults4 = countFaults(fifoPages(refs, 4));
    std::cout << "经典访问串: 1 2 3 4 1 2 5 1 2 3 4 5\n";
    std::cout << "FIFO 3 帧缺页次数: " << faults3 << "\n";
    std::cout << "FIFO 4 帧缺页次数: " << faults4 << "\n";
    if (faults4 > faults3) {
        std::cout << "结论: 物理块数增加后缺页次数反而增加，Belady 异常成立。\n";
    } else {
        std::cout << "结论: 本次未出现 Belady 异常。\n";
    }
}

void interactivePages() {
    const int frameCount = readInt("请输入内存块数: ", 1);
    const std::vector<int> refs = inputRefs();
    while (true) {
        std::cout << "\n页面置换算法:\n"
                  << "1. FIFO\n"
                  << "2. LRU\n"
                  << "3. OPT 最佳置换\n"
                  << "4. FIFO/LRU/OPT 全部对比\n"
                  << "5. Belady 异常验证\n"
                  << "0. 返回\n";
        const int choice = readInt("请选择: ", 0);
        if (choice == 0) return;
        if (choice == 1 || choice == 4) printPageResult("FIFO", fifoPages(refs, frameCount), frameCount);
        if (choice == 2 || choice == 4) printPageResult("LRU", lruPages(refs, frameCount), frameCount);
        if (choice == 3 || choice == 4) printPageResult("OPT", optPages(refs, frameCount), frameCount);
        if (choice == 5) runBeladyExperiment();
        if (choice < 1 || choice > 5) std::cout << "无效选项。\n";
    }
}

void runPageDemo() {
    std::cout << "\n========== 页面置换演示 ==========\n";
    const std::vector<int> refs{7, 0, 1, 2, 0, 3, 0, 4, 2, 3,
                                0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    const int frames = 3;
    printPageResult("FIFO", fifoPages(refs, frames), frames);
    printPageResult("LRU", lruPages(refs, frames), frames);
    printPageResult("OPT", optPages(refs, frames), frames);
    runBeladyExperiment();
    std::cout << "分析: OPT 是理论最优算法，用于衡量其他算法上界；LRU 利用局部性，"
              << "通常优于 FIFO；FIFO 简单但可能出现 Belady 异常。\n";
}

}  // namespace

void runMemoryDemo() {
    runPartitionDemo();
    runPageDemo();
}

void runMemoryMenu() {
    while (true) {
        std::cout << "\n========== 内存管理 ==========\n"
                  << "1. 动态分区管理 FF/BF/WF\n"
                  << "2. 页面置换 FIFO/LRU/OPT\n"
                  << "3. 内存管理综合演示\n"
                  << "0. 返回主菜单\n";
        const int choice = readInt("请选择: ", 0);
        switch (choice) {
            case 1:
                interactivePartition();
                break;
            case 2:
                interactivePages();
                break;
            case 3:
                runMemoryDemo();
                break;
            case 0:
                return;
            default:
                std::cout << "无效选项。\n";
                break;
        }
    }
}

}  // namespace osbasic
