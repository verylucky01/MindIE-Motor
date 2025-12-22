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

import collections
import time
import os
import json
import pandas as pd
from jsonlines import jsonlines
from prettytable import PrettyTable
import numpy as np

from .file_utils import PathCheck
from .results import ResultData
from .config import Config
from .logging import Logger
from .utils import get_time_stamp_ms


def generate_pretty_table(metrics: dict, col_titles: list) -> str:
    # 转换数据结构以适应prettytable
    rows = []
    for metric, values in metrics.items():
        # 添加指标名作为第一列
        rows.append([metric] + list(values.values()))
    # 创建PrettyTable对象并添加行数据
    x = PrettyTable()
    x.field_names = col_titles  # 添加列名
    x.add_rows(rows)
    x.align = 'r'
    return str(x)


class MetricsCalculator:
    def __init__(self, result: ResultData):
        self.result = result
        self.logger = Logger().logger

        def new_metric_result():
            return {"average": 0, "max": 0, "min": 0, "P75": 0, "P90": 0, "SLO_P90": 0, "P99": 0, "N": 0}

        self.metrics = collections.defaultdict(new_metric_result)

        def new_common_result():
            return {"Value": 0}

        self.common_metrics = collections.defaultdict(new_common_result)

    @staticmethod
    def __percentage(molecule: int, denominator: int):
        if denominator == 0:
            return '0'
        else:
            return str(round(molecule / denominator * 100, 2)) + '%'

    def calculate(self, config: Config, start_time: time):
        if os.getenv("MINDIE_LLM_BENCHMARK_ENABLE") == "1":
            self.__add_metrics_from_file()
        self.__calc_metrics()
        self.__calc_common_metrics(start_time, config)
        self.add_units()

    def save_to_csv(self, filename):
        # 获取当前时间戳
        timestamp = get_time_stamp_ms()
        # 创建文件名
        base_name, ext = os.path.splitext(filename)
        filename1 = f"{base_name}_perf_{timestamp}{ext}"
        if os.path.exists(filename1):
            ret, infos = PathCheck.check_path_full(filename1)
            if not ret:
                err_msg = f"Failed to check path {filename1} because {infos}."
                self.logger.error(f'[MIE01E000005] {err_msg}')
                raise ValueError(err_msg)

        filename2 = f"{base_name}_common_{timestamp}{ext}"
        if os.path.exists(filename2):
            ret, infos = PathCheck.check_path_full(filename2)
            if not ret:
                err_msg = f"Failed to check path {filename2} because {infos}."
                self.logger.error(f'[MIE01E000005] {err_msg}')
                raise ValueError(err_msg)
        # 平铺metrics字典并转换为DataFrame
        data1 = {k: v for k, v in self.metrics.items()}
        df1 = pd.DataFrame(data1)
        # 保存到CSV文件
        df1.to_csv(filename1, index=False)
        os.chmod(filename1, 0o640)
        # 平铺common_metrics字典并转换为DataFrame
        data2 = {k: v for k, v in self.common_metrics.items()}
        df2 = pd.DataFrame(data2)
        # 保存到CSV文件
        df2.to_csv(filename2, index=False)
        os.chmod(filename2, 0o640)

    def save_per_request_results(self, filename):
        timestamp = get_time_stamp_ms()
        base_name, ext = os.path.splitext(filename)
        filename = f"{base_name}_{timestamp}{ext}"
        if os.path.exists(filename):
            ret, infos = PathCheck.check_path_full(filename)
            if not ret:
                err_msg = f"Failed to check path {filename} because {infos}."
                self.logger.error(f'[MIE01E000005] {err_msg}')
                raise ValueError(infos)
        json_str = json.dumps(self.result.results_per_request, default=lambda k: k.__dict__)
        fd = os.open(filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o640)
        with os.fdopen(fd, "w") as json_file:
            json_file.write(json_str)

    def display_metrics_as_table(self):
        x = generate_pretty_table(self.metrics, ["Metric"] + list(self.metrics["FirstTokenTime"].keys()))
        # 打印表格
        self.logger.info("%sThe BenchMark test performance metric result is:%s%s", "\n", "\n", x)

    def display_common_metrics_as_table(self):
        x = generate_pretty_table(self.common_metrics,
                                  ["Common Metric"] + list(self.common_metrics["CurrentTime"].keys()))
        # 打印表格
        self.logger.info("%sThe BenchMark test common metric result is:%s%s", "\n", "\n", x)

    def add_units(self):
        ms = " ms"
        unit_token = " token/s"
        metrics_units_map = {
            'FirstTokenTime': ms,
            'DecodeTime': ms,
            'MaxDecodeTime': ms,
            'LastDecodeTime': ms,
            'GenerateTime': ms,
            'InputTokens': None,
            'GeneratedTokens': None,
            'GeneratedCharacters': None,
            'CharactersPerToken': None,
            'GeneratedTokenSpeed': unit_token,
            'PrefillBatchsize': None,
            'DecoderBatchsize': None,
            'QueueWaitTime': ' μs',
            'PostProcessingTime': ms,
            'Tokenizer': ms,
            'Detokenizer': ms,
            'ForwardTime': ms,
        }

        for metric, values in self.metrics.items():
            if metric not in metrics_units_map or metrics_units_map.get(metric) is None:
                continue
            for key, val in values.items():
                if key == 'N':
                    continue
                values[key] = str(val) + metrics_units_map.get(metric)
        common_metric_units_map = {
            'CurrentTime': None,
            'DataSource': None,
            'Failed': None,
            'Returned': None,
            'Total': None,
            'Concurrency': None,
            'ModelName': None,
            'Throughput': ' req/s',
            "lpct": ' ms',
            'TimeElapsed': ' s',
            'OutputGenerateSpeed': unit_token,
            'InputGenerateSpeed': unit_token,
            'TotalGenerateSpeed': unit_token,
            'GenerateSpeedPerClient': unit_token,
            'accuracy': None
        }

        for metric, values in self.common_metrics.items():
            if metric not in common_metric_units_map or common_metric_units_map.get(metric) is None:
                continue
            for key, val in values.items():
                values[key] = str(val) + common_metric_units_map.get(metric)

    def __add_metrics_from_file(self):
        log_path = os.getenv("MINDIE_LLM_BENCHMARK_FILEPATH",
                            os.path.join(os.getenv("MINDIE_LLM_HOME_PATH"), "logs", "benchmark.jsonl"))
        ret, infos = PathCheck.check_path_full(log_path)
        if not ret:
            self.logger.warning(f"[MIE01W000005] Failed to read LLM log, info: {infos}")
            return
        with jsonlines.open(log_path) as reader:
            for line in reader:
                if line["request_id"] in self.result.request_id_dict.keys():
                    self.result.forward_time.append(line["forward"])
                    self.result.log_post_processing_time.append(line["sample"])

    def __statistic_prefill_or_decode_batch_size(self, batch_sizes: list):
        # 统计逻辑为：
        # [2, 2, 5, 5, 3, 3, 5, 5, 5, 3] --> [2, 5, 3]
        # 即 根据第一个出现的数字X, 合并出现相同X次数的数字X
        # 比如第一次为5, 合并后续出现 4次5： [5,5,3,5,5,2,5...] 合并为 [5,3,2...]
        # 根据纪要, 暂不处理异常情况
        if not batch_sizes:
            return []
        statistics = []
        count_dict = {}

        for batch_size in batch_sizes:
            if batch_size in count_dict:
                count_dict[batch_size] = count_dict[batch_size] - 1
                if count_dict[batch_size] == 0:
                    del count_dict[batch_size]
            else:
                count_dict[batch_size] = batch_size - 1
                if count_dict[batch_size] == 0:
                    del count_dict[batch_size]
                statistics.append(batch_size)
        if len(count_dict) != 0:
            self.logger.warning('[MIE01W000005] Batch size is not fully compressed %s', count_dict)
        return statistics

    def __calc_metrics(self):
        slo = "SLO_P90"
        # 计算每个请求的时延平均
        per_request_avg_decode_time = []
        # DecodeBatchSize在slo情况下是按请求算的，每个请求的组的batchsize可能不一样，故需按请求处理
        per_request_avg_decode_batch_size = []

        for _, per_request_data in self.result.results_per_request.items():
            for per_request_metric, values in per_request_data.items():
                if per_request_metric == "latency" and len(values) > 1:
                    per_request_avg_decode_time.append(round(np.average(values[1:]), 4))
                if per_request_metric == "decode_bsz" and len(values) > 0:
                    per_request_avg_decode_batch_size.append(round(np.average(values)))

        # 所有的 N 均为定值
        data = self.result.convert_result()
        for metric, value in data.items():
            average_value, min_value, max_value, p75, p90, p99 = 0, 0, 0, 0, 0, 0
            decode_time_slo_p90 = 0
            
            # 如果是 PrefillBatchsize 和 DecoderBatchsize, 特殊计算
            if metric == 'PrefillBatchsize' or metric == 'DecoderBatchsize':
                if metric == 'DecoderBatchsize':
                    value = per_request_avg_decode_batch_size
                value = self.__statistic_prefill_or_decode_batch_size(value)
            if value:
                # 计算平均值
                average_value = round(np.average(value), 4)
                # 计算最小值
                min_value = round(min(value), 4)
                # 计算最大值
                max_value = round(max(value), 4)
                # 计算75分位值
                p75 = round(np.percentile(value, 75), 4)
                # 计算90分位值
                p90 = round(np.percentile(value, 90), 4)
                # 计算SLO_P90分位的Decode_Time值
                if per_request_avg_decode_time:
                    decode_time_slo_p90 = round(np.percentile(per_request_avg_decode_time, 90), 4)
                # 计算99分位值
                p99 = round(np.percentile(value, 99), 4)

            self.metrics[metric]["average"] = average_value
            self.metrics[metric]["min"] = min_value
            self.metrics[metric]["max"] = max_value
            self.metrics[metric]["P75"] = p75
            self.metrics[metric]["P90"] = p90
            self.metrics[metric]["P99"] = p99
            if metric == "DecodeTime":
                self.metrics[metric][slo] = decode_time_slo_p90
            else:
                self.metrics[metric][slo] = p90

        for key in self.metrics:
            self.metrics[key]["N"] = self.result.total_req
        self.__calc_char_per_token(data)

    def __calc_char_per_token(self, data: dict):
        slo = "SLO_P90"
        char_per_token = 'CharactersPerToken'
        generated_token = np.sum(data['GeneratedTokens'])
        average_value = round(np.sum(data['GeneratedCharacters']) / generated_token,
                              4) if generated_token != 0 else 0
        self.metrics[char_per_token]["average"] = average_value
        self.metrics[char_per_token]["min"] = '/'
        self.metrics[char_per_token]["max"] = '/'
        self.metrics[char_per_token]["P75"] = '/'
        self.metrics[char_per_token]["P90"] = '/'
        self.metrics[char_per_token][slo] = '/'
        self.metrics[char_per_token]["P99"] = '/'

    def __calc_common_metrics(self, start_time: time, config: Config):
        self.logger.info(f"calc_common_metrics start time: {start_time} !")
        value_key = "Value"
        self.logger.warning(f"[MIE01W000005] Empty response numbers: {self.result.empty_req} !")

        self.common_metrics['CurrentTime'][value_key] = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())

        if self.result.test_type == "engine":
            infer_time = self.result.engine_infer_time
        else:
            infer_time = self.result.total_time_elapsed
        self.common_metrics['TimeElapsed'][value_key] = round(infer_time, 4)
        self.common_metrics['DataSource'][value_key] = config.get_command_line_arg('DatasetPath')
        self.common_metrics['Failed'][value_key] = str(
            self.result.total_req - self.result.success_req) + '( ' + self.__percentage(
            self.result.total_req - self.result.success_req, self.result.total_req) + ' )'
        self.common_metrics['Returned'][value_key] = str(self.result.success_req) + '( ' + self.__percentage(
            self.result.success_req, self.result.total_req) + ' )'
        self.common_metrics['Total'][value_key] = str(self.result.total_req) + '[ ' + self.__percentage(
            self.result.total_req, self.result.total_req) + ' ]'
        concurrency_val = config.get_command_line_arg('Concurrency')
        self.common_metrics['Concurrency'][value_key] = concurrency_val
        self.common_metrics['ModelName'][value_key] = config.get_command_line_arg('ModelName')
        if sum(self.result.input_tokens_len) != 0:
            self.common_metrics['lpct'][value_key] = \
                round(sum(self.result.prefill_latencies) / sum(self.result.input_tokens_len), 4)
        else:
            self.common_metrics['lpct'][value_key] = 0
        try:
            self.common_metrics['Throughput'][value_key] = round(self.result.returned_req / infer_time, 4)
        except ZeroDivisionError:
            self.common_metrics['Throughput'][value_key] = 0
        except Exception as e:
            raise ValueError('Invalid result data') from e
        
        try:
            self.common_metrics['OutputGenerateSpeed'][value_key] = \
                round(sum(self.result.generate_tokens_len) / infer_time, 4)
            self.common_metrics['InputGenerateSpeed'][value_key] = \
                round(sum(self.result.input_tokens_len) / infer_time, 4)
            self.common_metrics['TotalGenerateSpeed'][value_key] = \
                round(sum(self.result.input_tokens_len + self.result.generate_tokens_len) / infer_time, 4)
            con_num = concurrency_val
            self.common_metrics['GenerateSpeedPerClient'][value_key] = \
                round(self.common_metrics['OutputGenerateSpeed'][value_key] / con_num, 4)
        except ZeroDivisionError:
            self.common_metrics['GenerateSpeedPerClient'][value_key] = 0
        except Exception as e:
            raise ValueError('Invalid result data') from e
        
        accuracy_str = "accuracy"
        if not config.get_command_line_arg('TestAccuracy') \
                or config.get_command_line_arg('DatasetType') in ['humaneval', 'boolq']:
            self.common_metrics[accuracy_str][value_key] = '/'
        else:
            if self.result.total_req == 0:
                self.logger.error("[MIE01E000005] Invalid infer output size : 0, cannot cal accuracy."
                                  " Please check your dataset.")
                self.common_metrics[accuracy_str][value_key] = '/'
            else:
                accuracy_num = " (" + str(self.result.correct_num) + "/" + str(self.result.total_req) + ")"
                self.common_metrics[accuracy_str][value_key] = str(self.__percentage(self.result.correct_num,
                                                                               self.result.total_req)) + accuracy_num
