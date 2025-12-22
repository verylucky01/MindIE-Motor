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
import sys
import json
import platform
import hashlib
import subprocess
import logging
from typing import List, Dict

THIRD_PARTY_ZIP_DIR = os.environ.get("THIRD_PARTY_ZIP_DIR", "zipped")   # 第三方库源代码保存路径
NO_CHECK_CERTIFICATE = os.environ.get("NO_CHECK_CERTIFICATE", "False")       # 使用wget下载是否加上 --no-check-certificate 选项
NO_CHECK_CERTIFICATE = NO_CHECK_CERTIFICATE.strip().lower() in ("1", "true")


def parse_dependencies(json_file_path):
    """Parse dependencies with given json file path."""
    dependencies = []
    try:
        # 读取 JSON 文件
        with open(json_file_path, "r", encoding="utf-8") as file:
            data = json.load(file)

        dependencies_str = "dependencies"
        # 检查 "dependencies" 字段是否存在且为列表
        if dependencies_str in data and isinstance(data[dependencies_str], list):
            dependencies = data[dependencies_str]
        else:
            logging.warning("Warning: 'dependencies' field is missing or not a list.")

    except FileNotFoundError:
        logging.error(f"Error: File '{json_file_path}' not found.")
    except json.JSONDecodeError as e:
        logging.error(f"Error: Failed to parse JSON file. {e}")
    except Exception as e:
        logging.error(f"Unexpected error: {e}")

    return dependencies


def preprocess_package_list(package_list: List) -> List:
    # 如果下载链接需要根据平台等处理，则在此时处理
    packages = []
    true_arch = platform.machine()
    fake_arch = r"${arch}"
    url_str = "url"
    for pack in package_list:
        if fake_arch in pack[url_str]:
            pack[url_str] = pack[url_str].replace(fake_arch, true_arch)
        packages.append(pack)
    return packages


def get_sha256sum(file_path):
    """Calculate the sha256 sum of given file."""
    sha256 = hashlib.sha256()
    try:
        with open(file_path, "rb") as f:
            # Read file in block, avoid big file need large memory.
            for chunk in iter(lambda: f.read(8192), b""):
                sha256.update(chunk)
        return sha256.hexdigest()
    except FileNotFoundError:
        logging.error("Error: File not found")
    except Exception as e:
        logging.error(f"Error: {e}")
    return ""


def check_integrity(file_path: str, sha256="") -> bool:
    """Check integrity of given file.

    Args:
        file_path (str): File path to calculate the sha256sum.
        sha256 (str, optional): Expected sha256sum of given file.

    Returns:
        bool: True if the file integrity check pass, else False.
    """
    if not sha256:
        logging.warning(f"WARN: Sha256sum for {file_path} is empty, skip to compare the integrity.")
        return True
    sha256_tmp = get_sha256sum(file_path)
    return sha256_tmp == sha256


def download_3rdparty(package_list: List[Dict]):
    def _download_3rdparty_one(package: Dict):
        save_full_path = os.path.join(THIRD_PARTY_ZIP_DIR, package["save_as"])
        sha256 = package.get("sha256sum", "")
        if os.path.exists(save_full_path):
            if not check_integrity(save_full_path, sha256):
                logging.info(f'{package["save_as"]} is damaged, remove it and download again.')
                os.remove(save_full_path)
            else:
                logging.info(f'{package["save_as"]} looks good, don\'t download it.')
                return
        url = package["url"]
        if not os.path.exists(THIRD_PARTY_ZIP_DIR):
            os.makedirs(THIRD_PARTY_ZIP_DIR, exist_ok=True)
        cmd = ["wget", "-c", url, "-O", save_full_path]
        if NO_CHECK_CERTIFICATE:
            cmd.append("--no-check-certificate")
        subprocess.run(cmd, check=True)
    for package in package_list:
        _download_3rdparty_one(package)


def check_package_download_and_status(package_list: List[Dict]):
    logging.info("\nCheck if the package is downloaded, and check if the package is good (not damaged).")
    out_str = "DOWNLOAD STATUS  FILENAME\tURL\n"
    for package in package_list:
        zip_filepath = os.path.join(THIRD_PARTY_ZIP_DIR, package["save_as"])
        sha256 = package.get("sha256sum", "")
        success = check_integrity(zip_filepath, sha256)
        out_str += "[YES]\t " if os.path.exists(zip_filepath) else "[NO]\t"
        out_str += "[YES]\t " if success else "[NO]\t"
        out_str += package["save_as"] + "   " + package["url"]
        out_str += "\n"
    logging.info(out_str)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        logging.info("Usage: python download_3rdparty.py <dependency_json_file_path>")
        sys.exit(1)
    logging.info(f"THIRD_PARTY_ZIP_DIR is {os.path.realpath(THIRD_PARTY_ZIP_DIR)}")

    json_file_path = sys.argv[1]
    package_list = parse_dependencies(json_file_path)
    package_list = preprocess_package_list(package_list)

    download_3rdparty(package_list)
    check_package_download_and_status(package_list)
