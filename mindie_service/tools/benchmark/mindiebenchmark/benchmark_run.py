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

import time
import os
import pwd
from gevent import monkey

from mindiebenchmark.common.config import Config
from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.output import MetricsCalculator
from mindiebenchmark.inference.benchmarker import GPTBenchmark
from mindiebenchmark.common.file_utils import FileOperator, PathCheck
from mindiebenchmark.common.env import init_logger
from mindiebenchmark.common.visual_utils import Visualization
from mindiebenchmark.common.utils import print_to_screen
from mindieclient.python.common.logging import Log



class BenchmarkRun():
    """A class to run benchmark.

    This class is the entrance to run benchmark, command line args inputted by user will be parsed,
    then execute the specific task.
    """
    def __init__(self):
        # 加载基础配置
        self.config = Config()
        # 加载命令行配置
        self.config.add_logger()
        self.config.validate_command_line_args()

        # 获取benchmark instance位置
        self.instance_path = self.config.get_config("INSTANCE_PATH")

        # 初始化日志类
        self.log = Logger()
        self.logger = self.log.logger

        self._record_current_operation()

        self.logger.info("The Benchmark init instance_path: %s", self.instance_path)
        # 初始化instance目录
        ret = FileOperator.create_dir(os.path.expanduser(self.instance_path))
        if not ret:
            raise ValueError("The Benchmark create instance dir error!!")
        self.results = []

    def run_engine(self):
        """Run benchmark in engine mode.

        When parameter --TestType from command line is 'engine', then benchmark runs in engine mode.
        This function is a high level interface and it finishes a complete inference.
        """
        benchmarker = GPTBenchmark(self.config)
        benchmarker.init_infer_engine()
        benchmarker.run()
        benchmarker.results.result_id = 0
        self.results.append((time.time(), benchmarker.results))
        self._save_result()
        benchmarker.infer_engine_finalize()
        self.log.set_log_file_permission(perm=0o440)
        Log(__name__).set_log_file_permission(perm=0o440)

    def run_client(self):
        """Run benchmark in client mode.

        When parameter --TestType from command line is 'client', 'tgi_client', 'vllm_client',
        'triton_client' or 'openai', then benchmark runs in client mode. This function is a high
        level interface and it finishes a complete inference.
        """
        run_n = self._get_run_num()
        for i in range(run_n):
            self._run_once(i)
        self._save_result()
        self.log.set_log_file_permission(perm=0o440)
        Log(__name__).set_log_file_permission(perm=0o440)

    def get_test_type(self):
        """Get the value of `--TestType` from command line.

        Returns:
            test_type (str): It should be one of {'engine', 'client', 'tgi_client', 'vllm_client',
                'triton_client', 'openai'}.
        """
        return self.config.get_command_line_arg("TestType")

    def _record_current_operation(self):
        self.logger.info("Running Task.")
        msg = "[UserId]: {}, [Ip]: localhost, [Model]: {}, [TaskType]: {}, [TestType]: {}, [DatasetType]: {}"
        user_name = pwd.getpwuid(os.getuid()).pw_name
        model_name = self.config.get_command_line_arg("ModelName")
        task_type = "Accuracy" if self.config.get_command_line_arg("TestAccuracy") else "Performance"
        test_type = self.config.get_command_line_arg("TestType")
        dataset_type = self.config.get_command_line_arg("DatasetType")
        msg = msg.format(user_name, model_name, task_type, test_type, dataset_type)
        if test_type != "engine":
            msg += ", [Http]: " + self.config.get_command_line_arg("Http")
        self.logger.info(msg)

    def _get_run_num(self):
        request_send_rate = self.config.command_line_args.get("RequestRate")
        if request_send_rate == "":
            return 1
        else:
            return len(request_send_rate)

    def _run_once(self, i: int):
        benchmarker = GPTBenchmark(self.config)
        # 执行 benchmarker 类的 run 接口
        benchmarker.results.result_id = i
        start_time = time.time()
        benchmarker.run()
        self.results.append((start_time, benchmarker.results))

    def _save_result(self):
        experiment_results = []
        vision_tool = Visualization(self.config)
        for index, (start_time, result) in enumerate(self.results):
            # 将计算结果保存到指定的结果文件
            metrics = MetricsCalculator(result)
            metrics.calculate(self.config, start_time)

            metrics.display_metrics_as_table()
            metrics.display_common_metrics_as_table()

            request_rate = self.config.get_command_line_arg("RequestRate")
            rate_element = request_rate[index] if request_rate else None
            vision_tool.draw_once(result, rate_element)
            experiment_results.append(vision_tool.get_experiment_results(metrics))

            save_path = self.config.get_command_line_arg("SavePath")
            try:
                metrics.save_to_csv(os.path.join(save_path, "result.csv"))
            except IOError as io_error:
                raise IOError("Failed to write file") from io_error
            except Exception as e:
                raise ValueError("Failed to save result to csv") from e
            metrics.save_per_request_results(os.path.join(save_path, 'results_per_request.json'))
        vision_tool.draw_multi_runs(experiment_results)


def main():
    # 加载命令行配置及校验
    Config.parse_command_line_args()

    # 获取config.json文件并校验
    config_file = Config.command_line_args.get("ConfigPath", Config.default_config_path)
    Config.load_config(config_file)
    Config.validate_config()

    # 初始化日志模块
    init_logger()
    # 启动benchmark
    print_to_screen("Please execute command `export MINDIE_LOG_TO_STDOUT=\"benchmark:1; client:1\"` "
          "if log message need to be displayed on screen next time.")
    benchmark_run = BenchmarkRun()
    print_to_screen("Benchmark task is running, please wait...")
    if benchmark_run.get_test_type() == "engine":
        benchmark_run.run_engine()
    else:
        benchmark_run.run_client()
    print_to_screen("Benchmark task completed successfully!")


if __name__ == "__main__":
    main()
