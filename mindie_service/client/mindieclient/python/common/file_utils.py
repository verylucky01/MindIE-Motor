# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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
from pathlib import Path

DEFAULT_MIN_FILE_SIZE = 1
DEFAULT_MAX_FILE_SIZE = 1073741824


def safe_open(file, *args, **kwargs):
    if not PathCheck.check_path_full(file):
        error_msg = f"Failed to open {file} %s"
        raise OSError(error_msg)
    return open(os.path.realpath(file), *args, **kwargs)
    
    
class PathCheck(object):

    @classmethod
    def check_path_full(cls, path_str: str, root_allowed: bool = True) -> Tuple[bool, str]:
        # 检查路径是否合法
        valid_flag, valid_msg = cls.check_path_valid(path_str)
        if not valid_flag:
            return valid_flag, valid_msg
        
        # 检查是否为软链接
        link_check, link_msg = cls.check_path_link(path_str)
        if not link_check:
            return link_check, link_msg
        
        # 检查路径是否存在
        exists_check, exists_msg = cls.check_path_exists(path_str)
        if not exists_check:
            return exists_check, exists_msg
        
        # 检查文件大小是否在范围内
        ret, infos = cls.check_file_size(path_str)
        if not ret:
            return ret, infos

        # 检查路径属组
        return cls.check_path_owner_group_valid(path_str, root_allowed)

    @classmethod
    def check_cert_path(cls, cert_path: str) -> Tuple[bool, str]:
        result, info = cls.check_path_full(cert_path)
        if not result:
            return result, info

        # 检查mode
        mode_check1, _ = cls.check_path_mode(0o400, cert_path)
        mode_check2, _ = cls.check_path_mode(0o600, cert_path)
        if mode_check1 or mode_check2:
            return True, ""

        error_msg = "Cert path mode should be 600 or 400."
        return False, error_msg

    @classmethod
    def check_path_valid(cls, file_path: str) -> Tuple[bool, str]:
        if not file_path:
            error_msg = "The path is empty."
            return False, error_msg
        if len(file_path) > 1024:
            error_msg = " The length of path exceeds 1024 characters."
            return False, error_msg

        invalid_chars = re.compile(r"[^0-9a-zA-Z_./-]")
        found_invalid = invalid_chars.findall(file_path)
        traversal_patterns = ["../", "..\\", ".."]
        has_traversal_attempt = any(pattern in file_path for pattern in traversal_patterns)
        
        if found_invalid or has_traversal_attempt:
            error_msg = "The path contains special characters."
            return False, error_msg
        return True, ""

    @classmethod
    def check_path_exists(cls, target_path: str) -> Tuple[bool, str]:
        if not os.path.exists(target_path):
            error_msg = "Path does not exist"
            return False, error_msg
        return True, ""

    @classmethod
    def check_path_link(cls, path: str) -> Tuple[bool, str]:
        try:
            normalized_path = os.path.normpath(path)
            absolute_path = os.path.abspath(normalized_path)
            path_components = Path(absolute_path).parts
            
            current_path = ""
            for component in path_components:
                current_path = os.path.join(current_path, component) if current_path else component

                if os.path.islink(current_path):
                    error_msg = f"The path component '{current_path}' is a soft link."
                    return False, error_msg
                    
            return True, ""
            
        except Exception as e:
            error_msg = f"Error checking path security: {str(e)}"
            return False, error_msg

    @classmethod
    def check_path_mode(cls, expected_mode: int, target_path: str) -> Tuple[bool, str]:
        # expected_mode为chmod命令的八进制权限，如chmod 640 file_path
        current_stat = os.stat(target_path)
        actual_mode = stat.S_IMODE(current_stat.st_mode)
        # 转换成八进制打印
        error_msg = f"Path mode check failed. Expected: {oct(expected_mode)}, Actual: {oct(actual_mode)}"
        if actual_mode != expected_mode:
            return False, error_msg
        return True, ""

    @classmethod
    def check_file_size(cls, file_path: str):
        try:
            # Open the file in binary read mode
            with open(file_path, "rb") as fp:
                # Seek to the end of the file
                fp.seek(0, os.SEEK_END)
                # Get the file size
                file_size = fp.tell()
            if file_size < DEFAULT_MIN_FILE_SIZE or file_size > DEFAULT_MAX_FILE_SIZE:
                err_msg = f"Read input file {file_path} failed, file size is invalid"
                return False, err_msg
            return True, ''
        except Exception as e:
            err_msg = f"Error: {str(e)}"
            return False, err_msg

    @classmethod
    def check_path_owner_group_valid(cls, target_path: str, allow_root: bool = True) -> Tuple[bool, str]:
        current_uid = os.getuid()
        current_gid = os.getgid()

        path_stat = os.stat(target_path)
        path_uid = path_stat.st_uid
        path_gid = path_stat.st_gid

        is_owner_match = path_uid == current_uid and path_gid == current_gid
        if allow_root:
            is_owner_match = is_owner_match or (path_uid == 0 and path_gid == 0)
        
        if is_owner_match:
            return True, ""
        error_msg = "Check the path owner and group failed."
        return False, error_msg