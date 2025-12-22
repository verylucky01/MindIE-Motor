#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import json
import os
import logging
import argparse
import fnmatch
import stat
import re
from typing import Dict, Optional, Tuple


logging.basicConfig(
    level=logging.INFO,
    format="[mindie] [%(asctime)s] [%(levelname)s] %(message)s",
    datefmt="%Y%m%d-%H:%M:%S"
)
logger = logging.getLogger(__name__)

MODEL_CONFIG_KEY = "ModelConfig"
MODEL_PARAM_KEY = "ModelParam"


class PathChecker(object):

    @classmethod
    def check_path_full(cls, path: str, allow_root: bool = True) -> Tuple[bool, str]:
        # 检查路径是否合法
        ret, info = cls.check_path_valid(path)
        if not ret:
            return ret, info
        # 检查是否为软链接
        ret, info = cls.check_symbolic(path)
        if not ret:
            return ret, info
        # 检查路径属组
        return cls.check_path_owner_group_valid(path, allow_root)

    @classmethod
    def check_path_valid(cls, path: str) -> Tuple[bool, str]:
        if len(path) > 1024:
            return False, "The length of path exceeds 1024 characters."
        if not path:
            return False, "The path is empty."
        pattern = re.compile(r"[^0-9a-zA-Z_./-]")
        match_name = pattern.findall(path)
        if match_name:
            return False, "The path contains special characters."
        if ".." in path:
            return False, "The path contains illegal '..' parent directory."
        return True, ""

    @classmethod
    def check_symbolic(cls, path: str) -> Tuple[bool, str]:
        if os.path.islink(os.path.normpath(path)):
            return False, "The path is a symbolic file."
        return True, ""

    @classmethod
    def check_path_mode(cls, given_mode: int, path: str) -> Tuple[bool, str]:
        cur_stat = os.stat(path)
        cur_mode = stat.S_IMODE(cur_stat.st_mode)
        if cur_mode != given_mode:
            return False, f"Check the path mode failed, expect {given_mode:o}, but got {cur_mode:o}."
        return True, ""

    @classmethod
    def check_path_owner_group_valid(cls, path: str, allow_root: bool = True) -> Tuple[bool, str]:
        user_id = os.getuid()
        cur_user_group_id = os.getgid()

        file_info = os.stat(path)
        file_owner_id = file_info.st_uid
        file_owner_group_id = file_info.st_gid

        flag = file_owner_id == user_id and file_owner_group_id == cur_user_group_id
        if allow_root:
            flag = flag or (file_owner_id == 0 and file_owner_group_id == 0)
        if flag:
            return True, ""
        return False, "Check the path owner and group failed."


def flatten_json(nested_dict: Dict, parent_key: str = "") -> Dict:
    """Flatten the key of json object as a path format. Example: {"key": {"sub_key1": 1, "sub_key2": 2}},
    The returned value after flatten is {"key/sub_key1": 1, "key/sub_key2": 2}
    """
    items = {}
    if isinstance(nested_dict, dict):
        for k, v in nested_dict.items():
            new_key = f"{parent_key}/{k}" if parent_key else k
            items.update(flatten_json(v, new_key))
    elif isinstance(nested_dict, list):
        # Check if contains dict
        if any(isinstance(i, dict) for i in nested_dict):
            for i, v in enumerate(nested_dict):
                new_key = f"{parent_key}[{i}]"
                items.update(flatten_json(v, new_key))
        else:   # If list has no dict, keep its as a value, e.g. "npuDeviceIds": [[0, 1, 2, 3]]
            items[parent_key] = nested_dict
    else:
        items[parent_key] = nested_dict  # Basic type (str, int, bool)
    return items


def unflatten_json(flat_dict: Dict) -> Dict:
    """Un-flatten the path format value as json. Example: {"key/sub_key1": 1, "key/sub_key2": 2},
    the returned value after flatten is {"key": {"sub_key1": 1, "sub_key2": 2}}
    """
    nested_dict = {}
    for key, value in flat_dict.items():
        parts = key.split('/')
        current = nested_dict
        for i, part in enumerate(parts):
            if '[' in part and ']' in part:
                # For list
                list_key, index = part[:-1].split('[')
                index = int(index)
                if list_key not in current:
                    current[list_key] = []
                # Make sure list is long enough
                while len(current[list_key]) <= index:
                    current[list_key].append({})
                if i == len(parts) - 1:
                    current[list_key][index] = value
                else:
                    current = current[list_key][index]
            else:
                # For dict
                if i == len(parts) - 1:
                    current[part] = value
                else:
                    if part not in current:
                        current[part] = {}
                    current = current[part]
    return nested_dict


def get_validated_list_field(obj, key) -> list:
    if key not in obj:
        logger.warning("%s not in object, empty list will be returned", str(key))
        return []
    field = obj[key]
    if not isinstance(field, list):
        raise RuntimeError(f"Field `{key}` is not a list")
    return field


def load_upgrade_info(upgrade_info_path: str, old_version: str, new_version: str) -> Optional[Dict]:
    """Parse upgrade info file to get upgrade rules."""
    upgrade_info = read_json(upgrade_info_path)
    # Using `fnmatch` to parse '*' in upgrade info file
    for upgrade in get_validated_list_field(upgrade_info, "upgrades"):
        if fnmatch.fnmatch(old_version, upgrade["from_version"]) and \
            fnmatch.fnmatch(new_version, upgrade["to_version"]):
            return upgrade
    return None


def check_file_path(path: str):
    safe_path = "?/" + "/".join(path.split("/")[-3:])
    if not os.path.exists(path):
        raise RuntimeError(f"Path doesn't exist: {safe_path}")
    if not os.path.isfile(path):
        raise RuntimeError(f"Path is not file: {safe_path}")
    ok, errmsg = PathChecker.check_path_full(path)
    if not ok:
        raise RuntimeError(f"Path check failed, reason: {errmsg}. Check path: {safe_path}")


def check_file_size(path: str, max_file_size: int = 10 * 1024 * 1024):
    try:
        file_size = os.path.getsize(path)
    except FileNotFoundError as e:
        raise RuntimeError(f"Path not exist or not a file: {path}") from e
    if file_size > max_file_size:
        raise RuntimeError(f"Invalid file size, should be no more than {max_file_size} but got {file_size}")


def check_path_owner_group_valid(path: str, is_support_root: bool = True):
    cur_user_id = os.getuid()
    cur_user_grp_id = os.getgid()

    file_info = os.stat(path)
    file_user_id = file_info.st_uid
    file_user_grp_id = file_info.st_gid

    flag = file_user_id == cur_user_id and file_user_grp_id == cur_user_grp_id
    if is_support_root:
        flag = flag or (file_user_id == 0 and file_user_grp_id == 0)
    if not flag:
        raise RuntimeError(f"Check the path owner and group failed: {path}")


def read_json(path: str) -> Dict:
    check_file_path(path)
    check_file_size(path)
    check_path_owner_group_valid(path)
    with open(path, "r", encoding="utf-8") as f:
        json_obj = json.load(f)
    return json_obj


def keep_old_model_config(old_flat, new_flat):

    def get_model_config_key_prefix(flat_config):
        model_config_key_prefix = None
        model_config_name = None
        for k in flat_config.keys():
            if MODEL_CONFIG_KEY in k or MODEL_PARAM_KEY in k:
                if MODEL_CONFIG_KEY in k:
                    model_config_key_prefix = k.split(MODEL_CONFIG_KEY)[0]
                    model_config_name = MODEL_CONFIG_KEY
                else:
                    model_config_key_prefix = k.split(MODEL_PARAM_KEY)[0]
                    model_config_name = MODEL_PARAM_KEY
                break
        return model_config_key_prefix, model_config_name

    # 获取 模型配置项前缀，及模型配置项名字
    new_model_config_key_prefix, model_config_name = get_model_config_key_prefix(new_flat)

    for old_k, old_v in old_flat.items():
        if MODEL_CONFIG_KEY not in old_k and MODEL_PARAM_KEY not in old_k:
            continue

        # 仅获取模型配置中的关键字
        old_model_config_key = old_k.split(MODEL_CONFIG_KEY)[-1] if MODEL_CONFIG_KEY in old_k \
                                                              else old_k.split(MODEL_PARAM_KEY)[-1]

        # 模型配置项中的参数保留些来，不做更改，如模型权重
        is_new_key = True
        for new_k in new_flat.keys():
            if model_config_name + old_model_config_key in new_k:
                new_flat[new_k] = old_v
                is_new_key = False
                break
        # 未在新配置项中的参数，会添加进去，保证升级后插件参数等保留下来
        if is_new_key:
            new_flat[new_model_config_key_prefix + model_config_name + old_model_config_key] = old_v


def keep_old_lora_modules(old_flat, new_flat):
    for old_key, old_value in old_flat.items():
        if fnmatch.fnmatch(old_key, "BackendConfig/ModelDeployConfig/LoraModules*"):
            new_flat[old_key] = old_value


def migrate_config(old_config_path: str, old_version: str, new_config_path: str, new_version: str,
                   upgrade_info_path: str) -> Dict:
    """Migrate config"""
    old_config = read_json(old_config_path)
    new_config = read_json(new_config_path)
    upgrade_info = load_upgrade_info(upgrade_info_path, old_version, new_version)
    if not upgrade_info:
        raise RuntimeError(f"No matched version upgrade info from {old_version} to {new_version}")
    # Load blacklist and whitelist
    blacklist = get_validated_list_field(upgrade_info, "blacklist")
    whitelist = get_validated_list_field(upgrade_info, "whitelist")
    old_flat = flatten_json(old_config)
    new_flat = flatten_json(new_config)
    keep_old_model_config(old_flat, new_flat)
    keep_old_lora_modules(old_flat, new_flat)
    migrated_flat = new_flat.copy()
    # Deal whitelist
    for key_mapping in whitelist:
        new_key = key_mapping["new_key"]
        old_key = key_mapping["old_key"]
        if old_key in old_flat:
            migrated_flat[new_key] = old_flat[old_key]
    unmigrated_flat = []
    # Deal common item (If new_key not in blacklist and whitelist, and in old config, merge it)
    for key in new_flat:
        if key not in blacklist and \
            key not in [pair.get("new_key") for pair in whitelist] and \
            key in old_flat:
            migrated_flat[key] = old_flat[key]
        else:
            unmigrated_flat.append(key)
    if unmigrated_flat:
        logger.warning("The following config can't be matched in ${MIES_INSTALL_PATH}/conf/config.json, please upgrade "
            "it manually if need: %s", str(list(map(lambda x: x.replace("/", "."), unmigrated_flat))))
    # Re-flatten config and return it
    migrated_config = unflatten_json(migrated_flat)
    return migrated_config


def save_json(json_obj: Dict, save_path: str):
    check_file_path(save_path)
    with open(save_path, "w", encoding="utf-8") as f:
        json.dump(json_obj, f, ensure_ascii=False, indent=4)


def parse_args():
    parser = argparse.ArgumentParser(description="Migrate configuration from old to new version.")
    parser.add_argument("--old_config_path", type=str, required=True, help="Path to the old configuration file.")
    parser.add_argument("--old_version", type=str, required=True, help="Old configuration version.")
    parser.add_argument("--new_config_path", type=str, required=True, help="Path to the new configuration file.")
    parser.add_argument("--new_version", type=str, required=True, help="New configuration version.")
    parser.add_argument("--upgrade_info_path", type=str, required=True, help="Path to the upgrade information file.")
    parser.add_argument("--save_path", type=str, default="", help="Path to the upgraded information file.")
    return parser.parse_args()


def main():
    args = parse_args()
    merged_config = migrate_config(args.old_config_path, args.old_version,
        args.new_config_path, args.new_version, args.upgrade_info_path)
    # Overwrite new config path if save path is not set
    save_path = args.save_path if args.save_path else args.old_config_path
    save_json(merged_config, save_path)


if __name__ == "__main__":
    logging.info("Begin upgrade mindie-server...")
    main()
    logger.info("Upgrade mindie-server finished.")
