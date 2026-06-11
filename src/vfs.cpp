#include "vfs.h"

#include "input_utils.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace osbasic {
namespace {

enum class AllocationMode {
    Contiguous = 1,
    Linked = 2,
    Indexed = 3,
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

std::string readToken(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    std::cin >> value;
    return value;
}

std::string readLineContent(const std::string& prompt) {
    std::cout << prompt;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string value;
    std::getline(std::cin, value);
    return value;
}

std::string modeName(AllocationMode mode) {
    if (mode == AllocationMode::Contiguous) return "连续分配";
    if (mode == AllocationMode::Linked) return "链接分配";
    return "索引分配";
}

struct FileEntry {
    std::string name;
    int size{};
    AllocationMode mode{AllocationMode::Contiguous};
    std::vector<int> dataBlocks;
    int indexBlock{-1};
};

class VirtualFileSystem {
public:
    VirtualFileSystem(int blockCount, int blockSize)
        : blockSize_(blockSize), bitmap_(blockCount, false), disk_(blockCount) {}

    void setMode(AllocationMode mode) {
        currentMode_ = mode;
        std::cout << "当前分配方式已切换为: " << modeName(currentMode_) << "\n";
    }

    bool createFile(const std::string& name) {
        if (files_.count(name) != 0) return false;
        files_[name] = FileEntry{name, 0, currentMode_, {}, -1};
        return true;
    }

    bool writeFile(const std::string& name, const std::string& content) {
        auto it = files_.find(name);
        if (it == files_.end()) return false;
        releaseFileBlocks(it->second);

        FileEntry next{name, static_cast<int>(content.size()), currentMode_, {}, -1};
        const int dataNeed = static_cast<int>((content.size() + blockSize_ - 1) / blockSize_);
        if (!allocateBlocks(next, dataNeed)) return false;

        int offset = 0;
        for (int block : next.dataBlocks) {
            disk_[block] = content.substr(offset, blockSize_);
            offset += blockSize_;
        }
        if (next.indexBlock != -1) {
            std::ostringstream ss;
            for (int block : next.dataBlocks) ss << block << " ";
            disk_[next.indexBlock] = ss.str();
        }
        it->second = next;
        return true;
    }

    bool appendFile(const std::string& name, const std::string& content) {
        std::string old;
        if (!readFile(name, old)) return false;
        return writeFile(name, old + content);
    }

    bool readFile(const std::string& name, std::string& out) const {
        auto it = files_.find(name);
        if (it == files_.end()) return false;
        std::ostringstream ss;
        for (int block : it->second.dataBlocks) ss << disk_[block];
        out = ss.str().substr(0, static_cast<std::size_t>(it->second.size));
        return true;
    }

    bool deleteFile(const std::string& name) {
        auto it = files_.find(name);
        if (it == files_.end()) return false;
        releaseFileBlocks(it->second);
        files_.erase(it);
        return true;
    }

    void listFiles() const {
        std::cout << "\n目录项:\n";
        if (files_.empty()) {
            std::cout << "(空目录)\n";
            return;
        }
        std::cout << std::left << std::setw(16) << "文件名" << std::setw(10) << "大小"
                  << std::setw(14) << "分配方式" << std::setw(10) << "索引块"
                  << "数据块\n";
        for (const auto& [name, file] : files_) {
            std::cout << std::left << std::setw(16) << name << std::setw(10) << file.size
                      << std::setw(14) << modeName(file.mode)
                      << std::setw(10) << (file.indexBlock == -1 ? "-" : std::to_string(file.indexBlock));
            for (int block : file.dataBlocks) std::cout << block << " ";
            std::cout << "\n";
        }
    }

    void fileInfo(const std::string& name) const {
        auto it = files_.find(name);
        if (it == files_.end()) {
            std::cout << "文件不存在。\n";
            return;
        }
        const FileEntry& f = it->second;
        std::cout << "\n文件信息:\n"
                  << "文件名: " << f.name << "\n"
                  << "大小: " << f.size << " 字节\n"
                  << "分配方式: " << modeName(f.mode) << "\n";
        if (f.indexBlock != -1) std::cout << "索引块: " << f.indexBlock << "\n";
        std::cout << "数据块: ";
        for (int block : f.dataBlocks) std::cout << block << " ";
        std::cout << "\n";
    }

    void printBitmap() const {
        std::cout << "\n空闲空间位图(0=空闲, 1=占用):\n";
        for (std::size_t i = 0; i < bitmap_.size(); ++i) {
            std::cout << (bitmap_[i] ? '1' : '0');
            if ((i + 1) % 8 == 0) std::cout << " ";
        }
        const int used = static_cast<int>(std::count(bitmap_.begin(), bitmap_.end(), true));
        const int free = static_cast<int>(bitmap_.size()) - used;
        std::cout << "\n已用块数: " << used << "/" << bitmap_.size()
                  << "，空闲块数: " << free
                  << "，块大小: " << blockSize_ << " 字节\n";
    }

private:
    bool allocateBlocks(FileEntry& file, int dataNeed) {
        if (file.mode == AllocationMode::Contiguous) return allocateContiguous(file, dataNeed);
        if (file.mode == AllocationMode::Linked) return allocateAny(file.dataBlocks, dataNeed);
        if (!allocateAny(file.dataBlocks, dataNeed)) return false;
        std::vector<int> index;
        if (!allocateAny(index, 1)) {
            releaseBlocks(file.dataBlocks);
            file.dataBlocks.clear();
            return false;
        }
        file.indexBlock = index.front();
        return true;
    }

    bool allocateContiguous(FileEntry& file, int dataNeed) {
        if (dataNeed == 0) return true;
        int runStart = -1, runLen = 0;
        for (int i = 0; i < static_cast<int>(bitmap_.size()); ++i) {
            if (!bitmap_[i]) {
                if (runStart == -1) runStart = i;
                ++runLen;
                if (runLen == dataNeed) {
                    for (int b = runStart; b < runStart + dataNeed; ++b) {
                        bitmap_[b] = true;
                        file.dataBlocks.push_back(b);
                    }
                    return true;
                }
            } else {
                runStart = -1;
                runLen = 0;
            }
        }
        return false;
    }

    bool allocateAny(std::vector<int>& out, int count) {
        if (static_cast<int>(std::count(bitmap_.begin(), bitmap_.end(), false)) < count) {
            return false;
        }
        for (int i = 0; i < static_cast<int>(bitmap_.size()) && count > 0; ++i) {
            if (!bitmap_[i]) {
                bitmap_[i] = true;
                out.push_back(i);
                --count;
            }
        }
        return true;
    }

    void releaseBlocks(const std::vector<int>& blocks) {
        for (int block : blocks) {
            bitmap_[block] = false;
            disk_[block].clear();
        }
    }

    void releaseFileBlocks(const FileEntry& file) {
        releaseBlocks(file.dataBlocks);
        if (file.indexBlock != -1) releaseBlocks({file.indexBlock});
    }

    int blockSize_;
    AllocationMode currentMode_{AllocationMode::Contiguous};
    std::vector<bool> bitmap_;
    std::vector<std::string> disk_;
    std::map<std::string, FileEntry> files_;
};

void demo() {
    std::cout << "\n========== 简易文件系统演示 ==========\n";
    VirtualFileSystem fs(24, 8);
    fs.createFile("contig.txt");
    fs.writeFile("contig.txt", "ABCDEFGHIJ");
    fs.setMode(AllocationMode::Linked);
    fs.createFile("link.txt");
    fs.writeFile("link.txt", "linked allocation blocks");
    fs.setMode(AllocationMode::Indexed);
    fs.createFile("index.txt");
    fs.writeFile("index.txt", "indexed allocation");
    fs.listFiles();
    fs.printBitmap();
    fs.fileInfo("index.txt");
    fs.deleteFile("link.txt");
    std::cout << "删除 link.txt 后:\n";
    fs.listFiles();
    fs.printBitmap();
    std::cout << "分析: 连续分配访问快但要求连续空间；链接分配扩展灵活但随机访问慢；"
              << "索引分配通过索引块记录数据块，兼顾随机访问和空间灵活性。\n";
}

}  // namespace

void runFileSystemDemo() {
    demo();
}

void runFileSystemMenu() {
    const int blocks = readInt("请输入磁盘块数: ", 1);
    const int blockSize = readInt("请输入块大小(字节): ", 1);
    VirtualFileSystem fs(blocks, blockSize);

    while (true) {
        std::cout << "\n========== 简易文件系统 ==========\n"
                  << "1. 创建文件\n"
                  << "2. 写文件\n"
                  << "3. 读文件\n"
                  << "4. 删除文件\n"
                  << "5. 显示目录\n"
                  << "6. 显示空闲空间位图\n"
                  << "7. 切换分配方式(连续/链接/索引)\n"
                  << "8. 追加文件内容\n"
                  << "9. 查看文件详细信息\n"
                  << "0. 返回主菜单\n";
        const int choice = readInt("请选择: ", 0);
        if (choice == 0) return;
        if (choice == 1) {
            const std::string name = readToken("文件名: ");
            std::cout << (fs.createFile(name) ? "创建成功。\n" : "创建失败：文件已存在。\n");
        } else if (choice == 2) {
            const std::string name = readToken("文件名: ");
            const std::string content = readLineContent("请输入写入内容: ");
            std::cout << (fs.writeFile(name, content) ? "写入成功。\n"
                                                      : "写入失败：文件不存在或空间不足。\n");
        } else if (choice == 3) {
            const std::string name = readToken("文件名: ");
            std::string content;
            if (fs.readFile(name, content)) std::cout << "文件内容: " << content << "\n";
            else std::cout << "读取失败：文件不存在。\n";
        } else if (choice == 4) {
            const std::string name = readToken("文件名: ");
            std::cout << (fs.deleteFile(name) ? "删除成功。\n" : "删除失败：文件不存在。\n");
        } else if (choice == 5) {
            fs.listFiles();
        } else if (choice == 6) {
            fs.printBitmap();
        } else if (choice == 7) {
            std::cout << "1. 连续分配  2. 链接分配  3. 索引分配\n";
            const int mode = readInt("请选择分配方式: ", 1);
            if (mode >= 1 && mode <= 3) fs.setMode(static_cast<AllocationMode>(mode));
            else std::cout << "无效分配方式。\n";
        } else if (choice == 8) {
            const std::string name = readToken("文件名: ");
            const std::string content = readLineContent("请输入追加内容: ");
            std::cout << (fs.appendFile(name, content) ? "追加成功。\n"
                                                       : "追加失败：文件不存在或空间不足。\n");
        } else if (choice == 9) {
            const std::string name = readToken("文件名: ");
            fs.fileInfo(name);
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

}  // namespace osbasic
