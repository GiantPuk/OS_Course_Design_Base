#include <iostream>
#include <limits>
#include <clocale>
#include <fstream>
#include <streambuf>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "memory.h"
#include "scheduler.h"
#include "sync_demo.h"
#include "vfs.h"
#include "input_utils.h"

namespace {

#ifdef _WIN32
class WindowsConsoleUtf8Buf : public std::streambuf {
public:
    explicit WindowsConsoleUtf8Buf(HANDLE handle) : handle_(handle) {}

protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        writeUtf8(s, static_cast<int>(n));
        return n;
    }

    int overflow(int ch) override {
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        const char c = static_cast<char>(ch);
        writeUtf8(&c, 1);
        return ch;
    }

private:
    void writeUtf8(const char* text, int length) {
        if (length <= 0) return;

        const int wideLength =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, nullptr, 0);
        if (wideLength <= 0) {
            DWORD written = 0;
            WriteFile(handle_, text, static_cast<DWORD>(length), &written, nullptr);
            return;
        }

        std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, wide.data(), wideLength);
        DWORD written = 0;
        WriteConsoleW(handle_, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
    }

    HANDLE handle_;
};

void installWindowsConsoleOutput() {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output == INVALID_HANDLE_VALUE || !GetConsoleMode(output, &mode)) {
        return;
    }

    static WindowsConsoleUtf8Buf consoleBuf(output);
    std::cout.rdbuf(&consoleBuf);
}
#endif

std::ofstream& debugLog() {
    static std::ofstream log("run_debug.log", std::ios::app);
    return log;
}

int readChoice() {
    int choice = -1;
    std::cout << "请选择: " << std::flush;
    if (!osbasic::readIntegerToken(choice)) {
        debugLog() << "readChoice failed, eof=" << std::cin.eof()
                   << ", fail=" << std::cin.fail() << "\n";
        if (std::cin.eof()) {
            return 0;
        }
        return -1;
    }
    return choice;
}

void runFullDemo() {
    std::cout << "\n========== 基础部分综合演示 ==========\n";
    osbasic::runSchedulerDemo();
    osbasic::runMemoryDemo();
    osbasic::runFileSystemDemo();
    std::cout << "\n同步并发模块需要观察线程交错输出，请在主菜单中单独运行。\n";
}

}  // namespace

int main() {
    std::setlocale(LC_ALL, "");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    installWindowsConsoleOutput();
#endif

    debugLog() << "main started\n";
    std::cout.setf(std::ios::unitbuf);

    std::cout << "操作系统课程设计基础部分模拟系统\n";
    std::cout << "覆盖：处理机调度 / 内存管理 / 进程同步 / 文件系统\n";

    while (true) {
        std::cout << "\n========== 主菜单 ==========\n"
                  << "1. 处理机调度\n"
                  << "2. 内存管理\n"
                  << "3. 进程同步与并发控制\n"
                  << "4. 简易文件系统\n"
                  << "5. 基础部分综合演示\n"
                  << "0. 退出\n"
                  << std::flush;

        const int choice = readChoice();
        switch (choice) {
            case 1:
                osbasic::runSchedulerMenu();
                break;
            case 2:
                osbasic::runMemoryMenu();
                break;
            case 3:
                osbasic::runSyncMenu();
                break;
            case 4:
                osbasic::runFileSystemMenu();
                break;
            case 5:
                runFullDemo();
                break;
            case 0:
                std::cout << "程序结束。\n";
                return 0;
            default:
                std::cout << "无效选项，请重新输入。\n";
                break;
        }
    }
}
