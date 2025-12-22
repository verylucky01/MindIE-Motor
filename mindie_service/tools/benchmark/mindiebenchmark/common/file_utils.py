#!/usr/bin/env python3
# coding=utf-8
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
import re
import stat

from typing import Tuple

from .logging import Logger


class PathCheck(object):

    @classmethod
    def check_path_full(cls, path: str, is_support_root: bool = True, max_file_size: int = 10 * 1024 * 1024,
                        is_size_check: bool = True) -> Tuple[bool, str]:
        check_elements = [cls.check_path_valid, cls.check_path_link, cls.check_path_exists]
        for check_element in check_elements:
            ret, infos = check_element(path)
            if not ret:
                return ret, infos
        ret, infos = cls.check_path_owner_group_valid(path, is_support_root)
        if not ret:
            return ret, infos
        if is_size_check:
            return cls.check_file_size(path, max_file_size)
        return True, ""


    @classmethod
    def check_directory_path(cls, path: str, is_support_root: bool = True) -> Tuple[bool, str]:
        ret, infos = cls.check_path_valid(path)
        if not ret:
            return ret, infos
        ret, infos = cls.check_path_link(path)
        if not ret:
            return ret, infos
        ret, infos = cls.check_path_owner_group_valid(path, is_support_root)
        if not ret:
            return ret, infos
        return True, ""

    # 检查路径是否合法
    @classmethod
    def check_path_valid(cls, path: str) -> Tuple[bool, str]:
        if not path:
            return False, "The path is empty."
        if len(path) > 1024:
            return False, " The length of path exceeds 1024 characters."

        pattern_name = re.compile(r"[^0-9a-zA-Z_./-]")
        match_name = pattern_name.findall(path)
        if match_name or ".." in path:
            return False, "The path contains special characters."

        return True, ""

    # 检查路径是否存在
    @classmethod
    def check_path_exists(cls, path: str) -> Tuple[bool, str]:
        if not os.path.exists(path):
            return False, "The path is not exists."
        return True, ""

    # 检查是否为软链接
    @classmethod
    def check_path_link(cls, path: str) -> Tuple[bool, str]:
        if os.path.islink(os.path.normpath(path)):
            return False, "The path is a soft link."
        return True, ""

    @classmethod
    def check_path_mode(cls, mode: int, path: str) -> Tuple[bool, str]:
        cur_stat = os.stat(path)
        cur_mode = stat.S_IMODE(cur_stat.st_mode)
        if cur_mode != mode:
            return False, "Check the path mode failed."
        return True, ""

    # 检查路径属组
    @classmethod
    def check_path_owner_group_valid(cls, path: str, is_support_root: bool = True) -> Tuple[bool, str]:
        cur_user_id = os.getuid()
        cur_user_grp_id = os.getgid()

        file_info = os.stat(path)
        file_user_id = file_info.st_uid
        file_user_grp_id = file_info.st_gid

        flag = file_user_id == cur_user_id and file_user_grp_id == cur_user_grp_id
        if is_support_root:
            flag = flag or (file_user_id == 0 and file_user_grp_id == 0)
        if flag:
            return True, ""
        error_strs = []
        if file_user_id != cur_user_id:
            error_strs.append("Check the path owner failed.")
        if file_user_grp_id != cur_user_grp_id:
            error_strs.append("Check the path group failed.")
        return False, " ".join(error_strs)

    @classmethod
    def check_system_path(cls, path: str) -> Tuple[bool, str]:
        system_paths = ["/usr/bin/", "/usr/sbin/", "/etc/", "/usr/lib/", "/usr/lib64/"]
        real_path = os.path.realpath(path)
        for sys_prefix in system_paths:
            if real_path.startswith(sys_prefix):
                return False, f"Invalid path, it is a system path."
        return True, ""

    @classmethod
    def check_file_size(cls, path: str, max_file_size: int = 10 * 1024 * 1024) -> Tuple[bool, str]:
        file_size = os.path.getsize(path)
        if file_size > max_file_size:
            return False, f"Invalid file size, should be no more than {max_file_size} but got {file_size}"
        return True, ""


class FileOperator(object):
    @staticmethod
    def set_path_permission(file_path: str, mode: int) -> bool:
        is_exist, msg = PathCheck.check_path_exists(file_path)
        if is_exist:
            os.chmod(file_path, mode)
            return True
        Logger().logger.warning(f'[MIE01W000000] {msg}')
        return False

    @classmethod
    def create_file(cls, file_path: str, mode: int = 0o640) -> bool:
        is_exist, _ = PathCheck.check_path_exists(file_path)
        if not is_exist:
            with os.fdopen(os.open(file_path, os.O_CREAT, mode), "w"):
                pass
        else:
            Logger().logger.warning(f'[MIE01W000000] File exists.')
        return cls.set_path_permission(file_path, mode)

    @classmethod
    def create_dir(cls, dir_path: str, mode: int = 0o750) -> bool:
        ret, infos = PathCheck.check_path_valid(dir_path)
        if not ret:
            Logger().logger.error(f'[MIE01E000000] {infos}')
            return ret
        ret, infos = PathCheck.check_system_path(dir_path)
        if not ret:
            Logger().logger.error(f'[MIE01E000000] {infos}')
            return ret

        os.makedirs(dir_path, mode=mode, exist_ok=True)
        ret, infos = PathCheck.check_path_owner_group_valid(dir_path)
        if not ret:
            Logger().logger.error(f'[MIE01E000000] {infos}')
            return ret
        return cls.set_path_permission(dir_path, mode)
