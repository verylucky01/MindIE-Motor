# coding=utf-8/# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import signal

from mindiebenchmark.common.logging import Logger


def signal_interrupt_handler(signum, frame):
    Logger().logger.info(f"Received SIGINT or SIGTERM signal with signum [{signum}]")
    while True:
        pid, status = os.waitpid(0, os.WNOHANG)
        if pid > 0:
            Logger().logger.info(f"Thread with pid {pid} is over, status {status}")
        else:
            break
    os.killpg(os.getpgrp(), signal.SIGKILL)


def signal_chld_handler(signum, frame):
    Logger().logger.info(f"Received SIGCHLD signal with signum [{signum}]")
    exit_flag = False
    while True:
        try:
            pid, status = os.waitpid(0, os.WNOHANG)
            if pid == 0:
                break
            Logger().logger.info(f"Thread with pid {pid} is over, status {status}")
            if not os.WIFEXITED(status):
                exit_flag = True
        except ChildProcessError:
            break
        except Exception:
            break
    if exit_flag:
        os.killpg(os.getpgrp(), signal.SIGKILL)


def register_signal():
    try:
        signal.signal(signal.SIGINT, signal_interrupt_handler)
        signal.signal(signal.SIGTERM, signal_interrupt_handler)
        signal.signal(signal.SIGCHLD, signal_chld_handler)
    except ValueError:
        Logger().logger.info("Error registering signal handlers.")
    except Exception as e:
        Logger().logger.info(f"An unexpected error occurred: {e}")
