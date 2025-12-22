/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "BackTrace.h"
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <fcntl.h>
#include <regex>
#include <fstream>
#include <string>
#include <unistd.h>
#include <mutex>
#include <execinfo.h>
#include "securec.h"

namespace {
    std::mutex g_backtraceLock;
    std::string g_filePath;                         // backTrace存放的路径
    const std::string g_pidMapsInfo("my_pid_info.txt"); // cat /proc/pid/maps 输出结果存放路径
}

std::string Regex2String(const std::string &s, const std::string &pattern)
{
    std::regex r(pattern);
    std::sregex_iterator pos(s.cbegin(), s.cend(), r);
    std::sregex_iterator end;
    return pos != end ? pos->str(1) : "";
}

void ExecuteCMD(const std::string &cmd, char *result, int32_t length)
{
    char buff[512];
    FILE *ptr = nullptr;
    if ((ptr = popen(cmd.c_str(), "r")) != nullptr) {
        while (fgets(buff, 512, ptr) != nullptr) { // 512表示buff的大小
            int32_t ret = strcat_s(result, length, buff);
            if (ret != 0) {
                return;
            }
        }
        pclose(ptr);
        ptr = nullptr;
    } else {
        std::cout << (std::string("ExecuteCMD error: ") + cmd) << std::endl;
    }
}


std::string GetLibAddress(const std::string &soName)
{
    std::ifstream fIn(g_pidMapsInfo.c_str());
    // 获取so在进程中映射的首地址
    std::string str;
    std::string minstr = "ffffffffffffffff";
    while (std::getline(fIn, str)) {
        auto ret = str.find(soName);
        if (ret == std::string::npos) {
            continue;
        }
        auto num1 = str.find_first_not_of(" ");
        auto num2 = str.find("-");
        auto str2 = str.substr(num1, num2);
        minstr = min(str2, minstr);
    }
    fIn.close();
    return "0x" + minstr;
}

void ParseOneLine(const std::string &input, const std::string &execName, int32_t number)
{
    std::cout << "\nline " << number << " : " << input << std::endl;
    // 解析异常调用栈的log内，括号中的符号名
    std::string pattern = "\\((.+)\\+0x";
    std::string functionName = Regex2String(input, pattern);
    if (functionName.size() != 0) {
        // 得到具体函数名
        char res1[512] = {""};
        ExecuteCMD("c++filt " + functionName, res1, sizeof(res1));
        std::cout << "function : " << functionName << " ---> " << res1;
    }
    //  获取异常调用栈的log内，[]中的地址
    pattern = "\\[(0x[0-9a-fA-F]+)\\]";
    std::string address = Regex2String(input, pattern);

    auto ret = input.find(execName);
    if (ret != std::string::npos) {
        // 可执行程序
        std::string cmdstr = "addr2line -e " + execName + " " + address;
        std::cout << cmdstr << std::endl;
        system(cmdstr.c_str());
    } else {
        auto num1 = input.find_first_not_of("0123456789 ");
        auto num2 = input.find("(");
        std::string sopath = input.substr(num1, num2);
        std::string soname = sopath.substr(sopath.find_last_of('/') + 1);
        auto libAddress = GetLibAddress(soname);
        if (libAddress == std::string("0xffffffffffffffff")) {
            std::cout << "doesn't find the lib address\n";
            return;
        }
        int32_t address1 = std::strtoll(libAddress.c_str(), nullptr, 0);
        int32_t address2 = std::strtoll(address.c_str(), nullptr, 0);
        char buf[32] = {0};
        auto ret = sprintf_s(buf, sizeof(buf), "0x%llX", address2 - address1);
        if (ret == -1) {
            std::cout << "sprintf_s failed " << std::endl;
            return;
        }
        std::string cmdstr = "addr2line -e " + sopath + " " + buf;
        std::cout << cmdstr << std::endl;
        system(cmdstr.c_str());
    }
}

void RegexBackTraceLog(const std::string &path)
{
    std::cout << "\n  ============= parse backtrace log ==============  \n";
    // 获取可执行程序的名字
    char szPath[256] = {""};
    readlink("/proc/self/exe", szPath, sizeof(szPath) - 1);
    std::cout << "execPath  : " << szPath << std::endl;
    std::string execName = szPath;
    execName = execName.substr(execName.find_last_of('/') + 1);
    std::cout << "execName  : " << execName << std::endl;
    std::ifstream fIn(path.c_str());

    std::string str;
    int32_t number = 0;
    while (std::getline(fIn, str)) {
        ParseOneLine(str, execName, number);
        number++;
    }
    fIn.close();
}

void SystemErrorHandler(int32_t signum)
{
    std::cout << "\n====== the abnormal signum num is: " << signum << " ======" << std::endl;
    std::lock_guard<std::mutex> lk(g_backtraceLock);
    std::string cmd = std::string("rm -rf ") + g_pidMapsInfo;
    system(cmd.c_str());
    cmd = std::string("rm -rf ") + g_filePath;
    system(cmd.c_str());
    int32_t pid = getpid();
    cmd = std::string("cat /proc/") + std::to_string(pid) + "/maps > " + g_pidMapsInfo;
    system(cmd.c_str());

    int32_t len = 1024;
    void *func[len];
    if (signal(signum, SIG_DFL) == SIG_ERR) {
        std::cout << "\n set SIG_DFL failed. \n";
    }
    size_t size = backtrace(func, len);
    int32_t fp = open(g_filePath.c_str(), O_RDWR | O_APPEND | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    backtrace_symbols_fd(func, size, fp);
    close(fp);
    char **funs = static_cast<char **>(backtrace_symbols(func, size));
    std::cout << "System error, Stack trace:\n";
    for (int32_t i = 0; i < size; ++i) {
        std::cout << i << " " << funs[i] << std::endl;
    }
    free(funs);
    RegexBackTraceLog(g_filePath);

    std::cout << "\n =========    the prosess info    =======\n";
    cmd = std::string("cat /proc/") + std::to_string(pid) + "/maps";
    system(cmd.c_str());
}

uint32_t RegisterBackTrace(char *file)
{
    g_filePath = std::string(file);
    // Invaild memory address
    if (signal(SIGSEGV, SystemErrorHandler) == SIG_ERR) {
        return 1;
    }
    // 执行了非法指令，非法执行数据段或者堆栈溢出也有可能产生这个信号
    if (signal(SIGILL, SystemErrorHandler) == SIG_ERR) {
        return 1;
    }
    // 非法地址, 包括内存地址对齐(alignment)出错, 访问不属于自己存储空间或只读存储空间
    if (signal(SIGBUS, SystemErrorHandler) == SIG_ERR) {
        return 1;
    }
    // 发生致命的算术运算错误时发出. 不仅包括浮点运算错误,溢出及除数为0等
    if (signal(SIGFPE, SystemErrorHandler) == SIG_ERR) {
        return 1;
    }
}