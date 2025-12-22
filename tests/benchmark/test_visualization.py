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

import unittest
from unittest.mock import MagicMock, patch

from matplotlib import pyplot as plt
from mindiebenchmark.common.visual_utils import (
    Visualization, HistogramConfig, FileOperator, ScatterPlotConfig
)


class TestVisualization(unittest.TestCase):

    def setUp(self):
        # 模拟配置
        self.config_mock = MagicMock()
        self.config_mock.get_command_line_arg.return_value = "/path/to/save"
        self.mock_savefig = patch("matplotlib.pyplot.savefig").start()
        # 模拟结果数据
        self.result_mock = MagicMock()
        self.result_mock.decode_batch_size = [32, 64, 128]
        self.result_mock.prefill_batch_size = [16, 32, 64]
        self.result_mock.decode_token_latencies = [100, 200, 300]
        self.result_mock.prefill_latencies = [120, 250, 350]
        self.result_mock.prefill_token_latencies = [120, 250, 350]
        self.result_mock.queue_wait_time = [5, 10, 15]
        self.result_mock.tokenizer_time = [50, 100, 150]
        self.result_mock.detokenizer_time = [60, 110, 160]
        self.result_mock.post_processing_time = [70, 120, 170]
        self.result_mock.input_tokens_len = [1000, 2000, 3000]

        # 初始化可视化对象
        self.visualization = Visualization(self.config_mock)

    def test_save_histogram(self):
        # 模拟数据
        config = HistogramConfig(
            filename="test_histogram.png",
            save_path="/path/to/save",
            title="Test Histogram",
            xlabel="Batch Size",
            x_unit="Items"
        )
        FileOperator.create_dir = MagicMock(return_value=True)
        self.visualization.logger.warning = MagicMock()
        # 假设生成的数据
        data = [100, 200, 300, 400, 500]

        # 测试保存直方图
        self.visualization.save_histogram(config, data)

        # 检查是否调用了 create_dir 和保存文件
        self.visualization.save_histogram(config, data)
        FileOperator.create_dir.assert_called_with("/path/to/save/visual_imgs")
        self.visualization.logger.warning.assert_not_called()

    def test_save_scatter_plot(self):
        # 模拟散点图配置
        config = ScatterPlotConfig(
            filename="test_scatter.png",
            save_path="/path/to/save",
            title="Test Scatter Plot",
            xlabel="Decode Batch Size",
            ylabel="Decode Token Latencies",
            x_unit="Items",
            y_unit="Milliseconds"
        )
        FileOperator.create_dir = MagicMock(return_value=True)
        # 测试保存散点图
        self.visualization.save_scatter_plot(
            x_data=[32, 64, 128],
            y_data=[100, 200, 300],
            config=config
        )

        # 检查保存路径
        FileOperator.create_dir.assert_called_with("/path/to/save/visual_imgs")

    def test_get_experiment_results(self):
        # 假设指标数据
        metrics_mock = MagicMock()
        metrics_mock.metrics = {
            'DecoderBatchsize': {'average': 64},
        }
        value_str = 'Value'
        metrics_mock.common_metrics = {
            'Throughput': {value_str: 1000},
            'GenerateSpeed': {value_str: 500},
            'Time_Elapsed': {value_str: 120},
        }

        # 测试结果提取
        result = self.visualization.get_experiment_results(metrics_mock)

        self.assertEqual(result[self.visualization.avg_batch_size_str], 64)
        self.assertEqual(result[self.visualization.query_per_second_str], 1000)
        self.assertEqual(result[self.visualization.token_per_second_str], 500)
        self.assertEqual(result[self.visualization.time_elapsed_str], 120)

    @patch.object(FileOperator, 'create_dir', return_value=True)
    def test_draw_once(self, file_operator_mock):
        # 测试单次绘制图表
        rate_element = "Test_Rate"
        self.visualization.draw_once(self.result_mock, rate_element)
        # 检查保存路径是否被调用
        file_operator_mock.assert_called()

    @patch.object(FileOperator, 'create_dir', return_value=True)
    def test_draw_multi_runs(self, file_operator_mock):
        mock_data = [1, 2, 3, 4]
        self.visualization.draw_multi_runs(mock_data)
        file_operator_mock.assert_called()

    def tearDown(self):
        # 清理可能产生的文件等
        plt.close('all')


if __name__ == '__main__':
    unittest.main()
