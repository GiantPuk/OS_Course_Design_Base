# 操作系统课程设计基础部分

本项目对应《操作系统》课程设计基础必做部分（70%），使用 C++17 实现一个菜单式操作系统核心机制模拟系统。内容覆盖处理机调度、内存管理、进程同步与并发控制、简易文件系统四个模块。

## 目录结构

```text
os_basic_course_design/
├── Makefile
├── README.md
├── include/
│   ├── scheduler.h
│   ├── memory.h
│   ├── sync_demo.h
│   └── vfs.h
├── src/
│   ├── main.cpp
│   ├── scheduler.cpp
│   ├── memory.cpp
│   ├── sync_demo.cpp
│   └── vfs.cpp
├── tests/
│   └── demo_input.txt
└── report/
    └── 实验报告提纲.md
```

## 编译与运行

Linux 环境：

```bash
make
./os_basic
```

Windows MinGW 环境也可直接编译：

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pthread -finput-charset=UTF-8 -fexec-charset=UTF-8 -Iinclude -o os_basic.exe src/main.cpp src/scheduler.cpp src/memory.cpp src/sync_demo.cpp src/vfs.cpp
```

自动运行基础综合演示：

```bash
./os_basic < tests/demo_input.txt
```

注意：本项目是多文件项目，不要只在 `src` 目录下执行 `g++ main.cpp -o main`。如果在 `src` 目录编译，至少需要：

```powershell
g++ -std=c++17 -finput-charset=UTF-8 -fexec-charset=UTF-8 -I..\include main.cpp scheduler.cpp memory.cpp sync_demo.cpp vfs.cpp -o main.exe
.\main.exe
```

## 中文乱码处理

程序启动时已经在 Windows 下设置控制台输入/输出为 UTF-8。若 PowerShell 仍显示乱码，请先执行：

```powershell
chcp 65001
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
```

推荐直接使用下面的测试脚本，脚本会自动设置 UTF-8。

## 四个基础模块测试脚本

每个测试脚本都会自动编译项目、运行对应模块的充实样例、保存 UTF-8 文本输出，并生成 SVG 图表到 `output/` 目录。

```powershell
cd "E:\大三下\os_basic\os_basic_course_design"
powershell -ExecutionPolicy Bypass -File tests\run_scheduler_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_memory_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_sync_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_filesystem_test.ps1
```

脚本输出文件：

- `output/scheduler_output.txt`：6 个进程下 FCFS/SJF/RR/Priority 的运行顺序和时间统计
- `output/memory_output.txt`：动态分区分配回收过程、FIFO/LRU 缺页统计
- `output/sync_output.txt`：生产者-消费者、读者-写者、哲学家进餐线程输出
- `output/filesystem_output.txt`：文件创建、写入、读取、删除、目录项和位图变化

生成图表：

- `output/scheduler_metrics.svg`：调度算法平均周转时间/平均等待时间对比
- `output/memory_partition_map.svg`：动态分区最终内存分布图
- `output/memory_page_replacement.svg`：FIFO/LRU 缺页次数和缺页率对比
- `output/sync_timeline.svg`：同步并发事件时间线
- `output/filesystem_bitmap.svg`：文件系统空闲空间位图变化

这些文本输出和 SVG 图可直接作为实验报告的结果截图或附录材料。

## 基础部分完成情况

### 1. 处理机调度

已实现 4 种典型调度算法：

- FCFS：先来先服务
- SJF：短作业优先，非抢占
- RR：时间片轮转，支持动态输入时间片
- Priority：优先级调度，数值越小优先级越高

支持动态输入进程的到达时间、服务时间、优先级；输出 Gantt 运行顺序、完成时间、周转时间、等待时间、平均周转时间和平均等待时间，并给出算法性能差异分析。

### 2. 内存管理

动态分区管理：

- 首次适应 FF
- 最佳适应 BF
- 支持作业申请、释放、分区合并
- 输出每一步分区表，展示内存分配与回收过程

页面置换：

- FIFO 页面置换
- LRU 页面置换
- 支持动态输入页面访问串和内存块数
- 输出每次访问后的内存块状态、缺页次数和缺页率

### 3. 进程同步与并发控制

使用 `std::thread`、`std::mutex`、`std::condition_variable` 和自定义计数信号量模拟：

- 生产者-消费者问题
- 读者-写者问题
- 哲学家进餐问题

实现中通过互斥锁保护临界区，通过条件变量/信号量控制同步关系，并在哲学家进餐问题中使用“服务员”信号量与 `std::lock` 避免死锁。

### 4. 文件系统

实现一个内存中的简易文件系统：

- 文件创建
- 文件写入
- 文件读取
- 文件删除
- 目录项显示
- 空闲空间位图显示

文件目录项保存文件名、文件大小和占用块号；空闲块使用位图管理；写入时按块分配，删除时释放块并回收目录项。

## 可用于报告的创新点

基础要求以外，本项目做了少量工程化增强：

- 统一菜单式入口，四个模块可以独立演示，也可以运行基础综合演示。
- 调度模块统一输出统计表，便于横向比较算法性能。
- 内存模块同时覆盖连续分配和分页置换两类机制。
- 文件系统保留目录项和位图两个核心数据结构，便于解释文件组织与空闲空间管理。

## 提交说明

课程要求报告中必须包含代码 URL 和访问方式。上传到 GitHub 或类似平台后，可在报告中填写：

```text
代码地址：https://github.com/<用户名>/<仓库名>
访问方式：公开仓库，可直接访问；或私有仓库，提交时提供教师可访问账号/邀请。
```

## 本轮基础部分增强说明

为提高基础 70% 部分的算法丰富度和报告材料质量，本项目在不转向自由扩展方向的前提下，补充了以下内容。

处理机调度现在包含 6 种算法：

- FCFS：先来先服务
- SJF：短作业优先，非抢占
- RR：时间片轮转
- Priority：非抢占式优先级调度
- SRTF：最短剩余时间优先，抢占式 SJF
- PriorityPreemptive：抢占式优先级调度

调度输出新增开始时间、响应时间、带权周转时间、CPU 利用率和吞吐量，更适合写实验分析。

内存管理现在包含：

- 动态分区：FF、BF、WF
- 分区统计：分区数、外部碎片数、最大空闲块、总空闲空间、内存利用率
- 页面置换：FIFO、LRU、OPT
- 页面指标：缺页次数、缺页率、命中率
- Belady 异常验证

文件系统现在包含：

- 创建、写入、读取、追加、删除
- 目录项、文件详细信息、空闲空间位图
- 连续分配、链接分配、索引分配三种磁盘块分配方式

新增图表：

- `output/scheduler_metrics.svg`
- `output/scheduler_weighted_turnaround.svg`
- `output/scheduler_rr_gantt.svg`
- `output/memory_partition_map.svg`
- `output/memory_page_replacement.svg`
- `output/memory_belady_anomaly.svg`
- `output/sync_timeline.svg`
- `output/filesystem_bitmap.svg`
- `output/filesystem_allocation_methods.svg`
# OS_Course_Design_Base
