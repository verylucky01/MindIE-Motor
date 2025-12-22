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
import time
import glob
import subprocess
from enum import Enum
import csv

import json
import logging


parent_dir = os.path.dirname(os.path.abspath(__file__))
logger_screen = logging.getLogger("test_print")
logger_screen.setLevel(logging.INFO)
ch = logging.StreamHandler()
logger_screen.addHandler(ch)


class PerfIndex(Enum):
    average = 0
    max = 1
    min = 2
    P75 = 3
    P90 = 4
    SLO_P90 = 5
    P99 = 6


def print_to_screen(msg: str) -> None:
    """Use logger to print on screen instead of print method
    """
    logger_screen.info(msg)


def load_config(path: str) -> dict:
    '''load test case parameters from test_cases.json
    '''
    with open(path, 'r') as file:
        data = json.load(file)
    return data


def is_valid_testcase_config(config: dict) -> bool:
    if not isinstance(config, dict):
        return False
    return True


def is_valid_compile_config(config: dict) -> bool:
    if not isinstance(config, dict):
        return False
    return True


def check_dir(folder_path: str) -> bool:
    if not os.path.isdir(folder_path):
        print_to_screen(f"{folder_path} is not a valid folder")
        return False
    return True


def get_latest_file_with_prefix(directory: str, prefix: str) -> str:
    '''find files with given prefix
    '''
    files = glob.glob(os.path.join(directory, prefix + '*'))

    if not files:
        return None

    # find latest file
    latest_file = max(files, key=os.path.getmtime)
    return latest_file


def get_pure_data(single_data: str) -> str:
    '''remove the unit of data
    '''
    special_chars = ['(', '[']
    pure_data = single_data.split(' ')[0]
    if pure_data[-1] in special_chars:
        pure_data = pure_data[:-1]
    return pure_data


def parse_table_from_csv(csv_path: str) -> dict:
    '''create a dictionary to store data
    '''
    data_dict = dict()

    # open csv file
    with open(csv_path, newline='') as csvfile:
        csvreader = csv.reader(csvfile)
        # get table header
        headers = next(csvreader)
        for header in headers:
            data_dict[header] = []

        for row in csvreader:
            for i, value in enumerate(row):
                key = headers[i]
                value = get_pure_data(value)
                data_dict[key].append(value)

    return data_dict


def extract_result_from_perf_csv(csv_dir: str) -> dict:
    """get performance information from result_perf.csv
    """
    res = dict()
    csv_path = get_latest_file_with_prefix(csv_dir, 'result_perf_')
    if csv_path is None:
        print_to_screen("No perf result is found!")
        return {}
    data = parse_table_from_csv(csv_path)

    first_token_slo_p90 = data["FirstTokenTime"][PerfIndex.SLO_P90.value]
    decode_slo_p90 = data["DecodeTime"][PerfIndex.SLO_P90.value]
    average_input_len = data["InputTokens"][PerfIndex.average.value]
    average_output_len = data["GeneratedTokens"][PerfIndex.average.value]
    res.update({
        "First Token SLO P90 /ms": first_token_slo_p90,
        "Decode SLO P90 /ms": decode_slo_p90,
        "Average Input Length /token": average_input_len,
        "Average Output Length /token": average_output_len})

    return res


def extract_result_from_common_csv(csv_dir: str) -> dict:
    """get performance and accuracy information from result_common.csv
    """
    accuracy_label = "accuracy"
    res = {"Accuracy": 0}
    csv_path = get_latest_file_with_prefix(csv_dir, 'result_common_')
    if csv_path is None:
        print_to_screen("No common result is found!")
        return res
    data = parse_table_from_csv(csv_path)

    accuracy = data[accuracy_label][0]
    throughput = data["Throughput"][0]
    generate_speed = data["GenerateSpeed"][0]
    res.update({
        "Accuracy": accuracy,
        "throughput req/s": throughput,
        "generate_speed token/s": generate_speed})

    return res


def gen_benchmark_cmd(config: dict, node_id: int = None) -> str:
    '''generate benchmark command from config
    '''
    benchmark_command = (f'benchmark '
        f'--DatasetPath "{config["DatasetPath"]}" '
        f'--DatasetType "{config["DatasetType"]}" '
        f'--ModelName "{config["ModelName"]}" '
        f'--ModelPath "{config["ModelPath"] if node_id is None else config["ModelPath"][node_id]}" '
        f'--TestType "{config.get("TestType", "client")}" '
        f'--TaskKind {config.get("TaskKind", "stream")} '
        f'--Concurrency {config["Concurrency"]} '
        f'--RequestRate {config["RequestRate"]} '
        f'--TestAccuracy {config.get("TestAccuracy", "false")} '
        f'--Http {config["Http"]} '
        f'--ManagementHttp {config["ManagementHttp"]} '
        f'--Tokenizer {config["Tokenizer"]} '
        f'--MaxOutputLen {config["MaxOutputLen"]} ')

    return benchmark_command


def get_time_stamp_ms() -> str:
    ct = time.time()
    local_time = time.localtime(ct)
    time_stamp = time.strftime("%Y%m%d%H%M%S", local_time)
    return time_stamp


def get_latest_commit_id():
    try:
        commit_id = subprocess.check_output(
            ['git', 'log', '--pretty=format:%H', '-n', '1'],
            stderr=subprocess.STDOUT,
            cwd=parent_dir
        ).strip()
        return commit_id.decode('utf-8')
    except subprocess.CalledProcessError as e:
        print_to_screen(f"Error occurred: {e.output.decode('utf-8')}")
        return ""


def save_results(data: list, csv_file: str):
    if len(data) == 0:
        print_to_screen(f"No data need to be written to {csv_file}")
        return

    # header
    fields = data[0].keys()

    file_exists = os.path.isfile(csv_file)

    # open file and write into file
    with open(csv_file, mode='a', newline='', encoding='utf-8') as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        # write header
        if not file_exists:
            writer.writeheader()
        # write data
        writer.writerows(data)

    print_to_screen(f"csv file has been saved as {csv_file}")
