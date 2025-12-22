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
#ifndef CONCURRENTTEST_H
#define CONCURRENTTEST_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

int32_t RunCmdBackGround(std::string command)
{
    std::cout << "RunCmdBackGround:" << command << std::endl;
    pid_t pid = fork(); // 创建一个子进程
    std::cout << "RunCmdBackGround pidpidpidpidpidpid:" << pid << std::endl;
    if (pid == -1) {
        std::cerr << "Error: fork() failed";
        return -1;
    }
    if (pid == 0) { // 子进程
        // 在子进程中执行指定命令
        if (execvp(command.c_str(), NULL) == -1) {
            std::cerr << "Error: execvp() failed";
            return -1;
        }
    } else { // 父进程
        // 等待子进程结束
        int status;
        waitpid(pid, &status, 0);
    }

    return 0;
}

void ExecuteCommand(std::string command)
{
    // 执行命令并捕获输出
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error popen failed." << std::endl;
        return;
    }

    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL) {
            result += buffer;
        }
    }

    pclose(pipe);

    // 输出命令执行结果
    std::cout << result << std::endl;
}
#endif