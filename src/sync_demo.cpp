#include "sync_demo.h"

#include "input_utils.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace osbasic {
namespace {

class CountingSemaphore {
public:
    explicit CountingSemaphore(int count) : count_(count) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return count_ > 0; });
        --count_;
    }

    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++count_;
        }
        cv_.notify_one();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

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

void pauseMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void producerConsumerDemo() {
    std::cout << "\n========== 生产者-消费者问题 ==========\n";
    const int bufferSize = 5;
    const int producerCount = 2;
    const int consumerCount = 2;
    const int itemsPerProducer = 6;

    std::deque<int> buffer;
    std::mutex bufferMutex;
    std::mutex outputMutex;
    CountingSemaphore emptySlots(bufferSize);
    CountingSemaphore fullSlots(0);
    int nextItem = 1;

    auto producer = [&](int id) {
        for (int i = 0; i < itemsPerProducer; ++i) {
            pauseMs(40 + id * 20);
            emptySlots.acquire();
            int item = 0;
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                item = nextItem++;
                buffer.push_back(item);
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Producer " << id << " 生产 " << item
                          << "，缓冲区大小=" << buffer.size() << "\n";
            }
            fullSlots.release();
        }
    };

    auto consumer = [&](int id) {
        while (true) {
            fullSlots.acquire();
            int item = 0;
            {
                std::lock_guard<std::mutex> lock(bufferMutex);
                item = buffer.front();
                buffer.pop_front();
            }
            emptySlots.release();
            if (item == -1) {
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Consumer " << id << " 收到结束标记，退出\n";
                break;
            }
            {
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Consumer " << id << " 消费 " << item << "\n";
            }
            pauseMs(70 + id * 15);
        }
    };

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    for (int i = 1; i <= consumerCount; ++i) consumers.emplace_back(consumer, i);
    for (int i = 1; i <= producerCount; ++i) producers.emplace_back(producer, i);
    for (auto& t : producers) t.join();

    for (int i = 0; i < consumerCount; ++i) {
        emptySlots.acquire();
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            buffer.push_back(-1);
        }
        fullSlots.release();
    }
    for (auto& t : consumers) t.join();
    std::cout << "结果: 使用 empty/full 计数信号量控制缓冲区容量，mutex 保护临界区。\n";
}

class ReaderWriterLock {
public:
    void lockRead() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return !writerActive_ && waitingWriters_ == 0; });
        ++activeReaders_;
    }

    void unlockRead() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --activeReaders_;
        }
        cv_.notify_all();
    }

    void lockWrite() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++waitingWriters_;
        cv_.wait(lock, [&] { return !writerActive_ && activeReaders_ == 0; });
        --waitingWriters_;
        writerActive_ = true;
    }

    void unlockWrite() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            writerActive_ = false;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int activeReaders_{0};
    int waitingWriters_{0};
    bool writerActive_{false};
};

void readersWritersDemo() {
    std::cout << "\n========== 读者-写者问题 ==========\n";
    ReaderWriterLock rwLock;
    std::mutex outputMutex;
    int sharedData = 0;

    auto reader = [&](int id) {
        for (int i = 0; i < 3; ++i) {
            pauseMs(30 + id * 15);
            rwLock.lockRead();
            {
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Reader " << id << " 读取 sharedData=" << sharedData << "\n";
            }
            pauseMs(50);
            rwLock.unlockRead();
        }
    };

    auto writer = [&](int id) {
        for (int i = 0; i < 2; ++i) {
            pauseMs(80 + id * 25);
            rwLock.lockWrite();
            ++sharedData;
            {
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Writer " << id << " 写入 sharedData=" << sharedData << "\n";
            }
            pauseMs(70);
            rwLock.unlockWrite();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 1; i <= 3; ++i) threads.emplace_back(reader, i);
    for (int i = 1; i <= 2; ++i) threads.emplace_back(writer, i);
    for (auto& t : threads) t.join();
    std::cout << "结果: 多个读者可并发读；写者独占访问；等待写者优先，减少写者饥饿。\n";
}

void diningPhilosophersDemo() {
    std::cout << "\n========== 哲学家进餐问题 ==========\n";
    constexpr int n = 5;
    std::vector<std::mutex> forks(n);
    std::mutex outputMutex;
    CountingSemaphore waiter(n - 1);

    auto philosopher = [&](int id) {
        const int left = id;
        const int right = (id + 1) % n;
        for (int round = 1; round <= 3; ++round) {
            {
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Philosopher " << id << " 思考，第 " << round << " 轮\n";
            }
            pauseMs(40 + id * 10);

            waiter.acquire();
            std::lock(forks[left], forks[right]);
            std::lock_guard<std::mutex> lockLeft(forks[left], std::adopt_lock);
            std::lock_guard<std::mutex> lockRight(forks[right], std::adopt_lock);
            {
                std::lock_guard<std::mutex> out(outputMutex);
                std::cout << "Philosopher " << id << " 拿到叉子 " << left << " 和 "
                          << right << "，开始进餐\n";
            }
            pauseMs(60);
            waiter.release();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < n; ++i) threads.emplace_back(philosopher, i);
    for (auto& t : threads) t.join();
    std::cout << "结果: 服务员信号量最多允许 4 人同时竞争叉子，配合 std::lock 避免死锁。\n";
}

}  // namespace

void runSyncMenu() {
    while (true) {
        std::cout << "\n========== 进程同步与并发控制 ==========\n"
                  << "1. 生产者-消费者\n"
                  << "2. 读者-写者\n"
                  << "3. 哲学家进餐\n"
                  << "4. 三个问题全部演示\n"
                  << "0. 返回主菜单\n";
        const int choice = readInt("请选择: ", 0);
        switch (choice) {
            case 1:
                producerConsumerDemo();
                break;
            case 2:
                readersWritersDemo();
                break;
            case 3:
                diningPhilosophersDemo();
                break;
            case 4:
                producerConsumerDemo();
                readersWritersDemo();
                diningPhilosophersDemo();
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
