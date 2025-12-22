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
import math
from dataclasses import dataclass
from typing import Tuple, List, Optional
from decimal import Decimal

import numpy as np
import matplotlib.pyplot as plt

from .logging import Logger
from .file_utils import FileOperator


@dataclass
class HistogramConfig:
    '''
    - filename (str): 文件名
    - save_path (str): 保存路径
    - xlabel (str): x轴标签
    - xlabel (str): x轴标签
    - ylabel (str): y轴标签
    - bins (int): bin(箱子)的个数
    - title (str): 图表标题
    - figsize (tuple): 图表大小
    '''
    filename: str
    save_path: str
    title: str
    xlabel: str
    ylabel: str = 'Frequency'
    x_unit: str = ''
    bins: int = 200
    figsize: Tuple[int, int] = (12, 6)


@dataclass
class ScatterPlotConfig:
    '''
    - filename (str): 文件名
    - save_path (str): 保存路径
    - xlabel (str): x轴标签
    - xlabel (str): x轴标签
    - ylabel (str): y轴标签
    - title (str): 图表标题
    - figsize (tuple): 图表大小
    - color (str): 散点颜色
    - alpha (float): 散点透明度
    - grid (bool): 是否显示网格
    - grid_style (str): 网格线样式
    - grid_alpha (float): 网格线透明度
    - xticks_rotation (int): x轴刻度标签旋转角度
    - x_key (str): 如果x_data是字典列表，用于x轴的数据在字典中的值。若x_data为list，该值设None
    - y_key (str): 如果y_data是字典列表，用于y轴的数据在字典中的值。若y_data为list，该值设None
    '''
    filename: str
    save_path: str
    title: str
    xlabel: str
    ylabel: str = 'Frequency'
    x_unit: str = ''
    y_unit: str = ''
    bins: int = 50
    figsize: Tuple[int, int] = (12, 6)
    color: str = 'blue'
    alpha: float = 0.6
    grid: bool = True
    grid_style: str = '--'
    grid_alpha: float = 0.7
    xticks_rotation: int = 45
    x_key: Optional[str] = None
    y_key: Optional[str] = None


class TitleTemplate:
    def __init__(self, template):
        self.template = template

    def format(self, *args, **kwargs):
        return self.template.format(*args, **kwargs)


class Visualization():

    def __init__(self, config):
        # 获取保存路径
        self.save_path = config.get_command_line_arg("SavePath")

        # 画图标题以及坐标轴名字定义
        self.decode_batch_size_str = 'Decoder_Batch_Size'
        self.prefill_batch_size_str = 'Prefill_Batch_Size'
        self.decode_token_latencies_str = 'Decode_Token_Latencies'
        self.prefill_token_latencies_str = 'Prefill_Token_Latencies'
        self.queue_wait_time_str = 'Queue_Wait_Time'
        self.post_processing_latencies_str = 'Post_Processing_Latencies'
        self.tokenizer_str = 'Tokenizer_Time'
        self.detokenizer_str = 'Detokenizer_Time'
        self.avg_batch_size_str = 'AVG_Batch_Size'
        self.token_per_second_str = 'Token_Per_Second'
        self.prefill_token_per_second_str = 'Prefill_Token_Per_Second'
        self.query_per_second_str = 'Query_Per_Second'
        self.prefill_sequence_len_str = 'Prefill_Sequence_Length'
        self.time_elapsed_str = 'Time_Elapsed'
        self.frequency_str = 'Frequency'

        self.millisecond_unit = 'Milliseconds (ms)'
        self.second_unit = 'Seconds (s)'
        self.batch_size_unit = 'Items'
        self.token_throughput_unit = 'token/s'
        self.request_throughput_unit = 'req/s'
        self.token_units = 'token'

        self.histogram_title = TitleTemplate('{}_Histogram_of_{}_{}')
        self.scatter_plot_title = TitleTemplate('Scatter_Plot_of_{}_vs_{}')

        self.logger = Logger().logger

    def save_histogram(self, config: HistogramConfig, data, folder_name='visual_imgs', lowest_per=0.01):
        """
        保存直方图的函数

        参数：
        - data (list): 用于绘制直方图的数据列表
        - config (HistogramConfig): 直方图详细配置
        """
        if len(data) == 0 or (None in data):
            self.logger.warning(f"[MIE01W000004] No data to draw histogram: {config.filename}, return. "
                                "Please verify which mode has the data to plot")
            return
        plt.figure(figsize=config.figsize)
        n, _, patches = plt.hist(data, bins=config.bins, edgecolor='black')

        plt.xlabel(f"{config.xlabel} ({config.x_unit})" if config.x_unit else config.xlabel)
        plt.ylabel(config.ylabel)
        plt.title(config.title)

        total = sum(n)
        for count, patch in zip(n, patches):
            height = patch.get_height()
            percentage = count / total

            # 仅显示百分之比大于lowest_per的条目
            if percentage >= lowest_per:
                plt.text(
                    patch.get_x() + patch.get_width() / 2,
                    height,
                    f'{percentage:.2f}',
                    ha='center',
                    va='bottom',
                    fontsize=10,
                )

        plt.xticks(rotation=45, ha='right')

        save_path = os.path.join(config.save_path, folder_name)
        ret = FileOperator.create_dir(save_path)
        if not ret:
            raise ValueError(f"Invalid SavePath, cannot creat this directory")
        img_path = os.path.join(save_path, config.filename)
        plt.tight_layout()
        plt.savefig(img_path)
        plt.close()

    def save_scatter_plot(self, x_data, y_data, config: ScatterPlotConfig, folder_name='visual_imgs'):
        """
        保存通用散点图的函数

        参数：
        - data (list): 用于绘制直方图的数据列表
        - x_data: x轴数据。可以是列表或包含字典的列表
        - y_data: y轴数据。可以是列表或包含字典的列表 (如果是字典列表，则与x_data相同)
        - config (ScatterPlotConfig): 散点图详细配置
        """
        if len(x_data) == 0:
            self.logger.warning(f"[MIE01W000004] No data to plot: {config.filename}, return."
                                "Please verify which mode has the data to plot")
            return
        if isinstance(x_data[0], dict) and config.x_key and config.y_key:
            x_values = [point[config.x_key] for point in x_data]
            y_values = [point[config.y_key] for point in x_data]
        elif isinstance(x_data, List) and isinstance(y_data, List):
            x_values = x_data
            y_values = y_data
        else:
            raise ValueError("数据格式不正确。请提供列表或包含适当键的字典列表")

        plt.figure(figsize=config.figsize)
        plt.scatter(x_values, y_values, alpha=config.alpha, color=config.color)
        plt.xlabel(f"{config.xlabel} ({config.x_unit})" if config.x_unit else config.xlabel)
        plt.ylabel(f"{config.ylabel} ({config.y_unit})" if config.y_unit else config.ylabel)
        plt.title(config.title)

        if config.grid:
            plt.grid(True, linestyle=config.grid_style, alpha=config.grid_alpha)

        plt.xticks(rotation=config.xticks_rotation, ha='right')

        save_path = os.path.join(config.save_path, folder_name)
        ret = FileOperator.create_dir(save_path)
        if not ret:
            raise ValueError(f"Invalid SavePath, cannot create this directory")
        img_path = os.path.join(save_path, config.filename)
        plt.tight_layout()
        plt.savefig(img_path)
        plt.close()

    def get_experiment_results(self, metrics):
        decoder_batchsize_str = 'DecoderBatchsize'
        throughput_str = 'Throughput'
        generate_speed_str = 'OutputGenerateSpeed'
        average_str = 'average'
        value_str = 'Value'
        avg_batch_size = metrics.metrics[decoder_batchsize_str][average_str]
        query_per_second = metrics.common_metrics[throughput_str][value_str]
        token_per_second = metrics.common_metrics[generate_speed_str][value_str]
        time_elapsed = metrics.common_metrics[self.time_elapsed_str][value_str]

        return {
            self.avg_batch_size_str: avg_batch_size,
            self.query_per_second_str: query_per_second,
            self.token_per_second_str: token_per_second,
            self.time_elapsed_str: time_elapsed
        }

    def draw_once(self, result, rate_element):
        '''
        每次实验都画的图
        - 直方图:
            - decode_batch_size
            - prefill_batch_size
            - decode_token_latencies
            - prefill_token_latencies
            - queue_wait_time
            - tokenizer_time
            - detokenizer_time
            - post_processing_latenciess
        - 散点图:
            - decode_batch_size_vs_decode_token_latencies
        '''
        decode_batch_size_title = self.histogram_title.format(
            self.frequency_str,
            self.decode_batch_size_str,
            rate_element
        )
        decode_batch_size_filename = f'{decode_batch_size_title}.png'
        decode_batch_size_config = HistogramConfig(
            xlabel=self.decode_batch_size_str,
            x_unit=self.batch_size_unit,
            filename=decode_batch_size_filename,
            save_path=self.save_path,
            title=decode_batch_size_title
        )
        self.save_histogram(decode_batch_size_config, result.decode_batch_size)

        prefill_batch_size_title = self.histogram_title.format(
            self.frequency_str,
            self.prefill_batch_size_str,
            rate_element
        )
        prefill_batch_size_filename = f'{prefill_batch_size_title}.png'
        prefill_batch_size_config = HistogramConfig(
            xlabel=self.prefill_batch_size_str,
            x_unit=self.batch_size_unit,
            filename=prefill_batch_size_filename,
            save_path=self.save_path,
            title=prefill_batch_size_title
        )
        self.save_histogram(prefill_batch_size_config, result.prefill_batch_size)

        decode_latencies_title = self.histogram_title.format(
            self.frequency_str,
            self.decode_token_latencies_str,
            rate_element
        )
        decode_latencies_filename = f'{decode_latencies_title}.png'
        decode_latencies_config = HistogramConfig(
            xlabel=self.decode_token_latencies_str,
            x_unit=self.millisecond_unit,
            filename=decode_latencies_filename,
            save_path=self.save_path,
            title=decode_latencies_title
        )
        self.save_histogram(decode_latencies_config, np.round(result.decode_token_latencies).astype(int))

        prefill_latencies_title = self.histogram_title.format(
            self.frequency_str,
            self.prefill_token_latencies_str,
            rate_element
        )
        prefill_latencies_filename = f'{prefill_latencies_title}.png'
        prefill_latencies_config = HistogramConfig(
            xlabel=self.prefill_token_latencies_str,
            x_unit=self.millisecond_unit,
            filename=prefill_latencies_filename,
            save_path=self.save_path,
            title=prefill_latencies_title
        )
        self.save_histogram(prefill_latencies_config, np.round(result.prefill_latencies).astype(int))

        queue_wait_time_title = self.histogram_title.format(
            self.frequency_str,
            self.queue_wait_time_str,
            rate_element
        )
        queue_wait_time_filename = f'{queue_wait_time_title}.png'
        queue_wait_time_config = HistogramConfig(
            xlabel=self.queue_wait_time_str,
            x_unit=self.millisecond_unit,
            filename=queue_wait_time_filename,
            save_path=self.save_path,
            title=queue_wait_time_title
        )
        self.save_histogram(queue_wait_time_config, np.round(result.queue_wait_time).astype(int))

        tokenizer_title = self.histogram_title.format(
            self.frequency_str,
            self.tokenizer_str,
            rate_element
        )
        tokenizer_filename = f'{tokenizer_title}.png'
        tokenizer_config = HistogramConfig(
            xlabel=self.tokenizer_str,
            x_unit=self.millisecond_unit,
            filename=tokenizer_filename,
            save_path=self.save_path,
            title=tokenizer_title
        )
        self.save_histogram(tokenizer_config, np.round(result.tokenizer_time).astype(int))

        detokenizer_title = self.histogram_title.format(
            self.frequency_str,
            self.detokenizer_str,
            rate_element
        )
        detokenizer_filename = f'{detokenizer_title}.png'
        detokenizer_config = HistogramConfig(
            xlabel=self.detokenizer_str,
            x_unit=self.millisecond_unit,
            filename=detokenizer_filename,
            save_path=self.save_path,
            title=detokenizer_title
        )
        self.save_histogram(detokenizer_config, np.round(result.detokenizer_time).astype(int))

        post_processing_latencies_title = self.histogram_title.format(
            self.frequency_str,
            self.post_processing_latencies_str,
            rate_element
        )
        post_processing_latencies_filename = f'{post_processing_latencies_title}.png'
        post_processing_latencies_config = HistogramConfig(
            xlabel=self.post_processing_latencies_str,
            x_unit=self.millisecond_unit,
            filename=post_processing_latencies_filename,
            save_path=self.save_path,
            title=post_processing_latencies_title
        )
        self.save_histogram(post_processing_latencies_config, np.round(result.post_processing_time).astype(int))

        decode_bs_vs_latencies_title = self.scatter_plot_title.format(
            self.decode_batch_size_str,
            self.decode_token_latencies_str
        )
        decode_bs_vs_latencies_filename = f"{decode_bs_vs_latencies_title}_{rate_element}.png"
        decode_bs_vs_latencies_config = ScatterPlotConfig(
            filename=decode_bs_vs_latencies_filename,
            save_path=self.save_path,
            title=decode_bs_vs_latencies_title,
            xlabel=self.decode_batch_size_str,
            ylabel=self.decode_token_latencies_str,
            x_unit=self.batch_size_unit,
            y_unit=self.millisecond_unit,
            figsize=(15, 6)
        )
        self.save_scatter_plot(
            x_data=result.decode_batch_size,
            y_data=np.round(result.decode_token_latencies).astype(int).tolist(),
            config=decode_bs_vs_latencies_config
        )

        prefill_sequence_len_latencies_title = self.scatter_plot_title.format(
            self.prefill_sequence_len_str,
            self.prefill_token_latencies_str
        )

        # prefill 相关的绘图
        if (None not in result.prefill_batch_size):
            prefill_sequence_len = [a * b for a, b in zip(result.input_tokens_len, result.prefill_batch_size)]
            prefill_sequence_len_latencies_filename = f"{prefill_sequence_len_latencies_title}_{rate_element}.png"
            prefill_sequence_len_latencies_config = ScatterPlotConfig(
                filename=prefill_sequence_len_latencies_filename,
                save_path=self.save_path,
                title=prefill_sequence_len_latencies_title,
                xlabel=self.prefill_sequence_len_str,
                ylabel=self.prefill_token_latencies_str,
                x_unit=self.token_units,
                y_unit=self.millisecond_unit,
                figsize=(15, 6)
            )
            self.save_scatter_plot(
                x_data=prefill_sequence_len,
                y_data=np.round(result.prefill_latencies).astype(int).tolist(),
                config=prefill_sequence_len_latencies_config
            )

            prefill_sequence_len_throughput_title = self.scatter_plot_title.format(
                self.prefill_sequence_len_str,
                self.prefill_token_per_second_str
            )
            prefill_throughput = []
            for a, b, c in zip(result.input_tokens_len, result.prefill_batch_size, result.prefill_latencies):
                if math.isclose(c, 0, rel_tol=0, abs_tol=Decimal('1e-8')):
                    self.logger.warning("[MIE01W000004] prefill_latency is close to 0, "
                                        "ignore to draw relevant figures.")
                    return
                prefill_throughput.append((a * b) / c)
            prefill_sequence_len_throughput_filename = f"{prefill_sequence_len_throughput_title}_{rate_element}.png"
            prefill_sequence_len_throughput_config = ScatterPlotConfig(
                filename=prefill_sequence_len_throughput_filename,
                save_path=self.save_path,
                title=prefill_sequence_len_throughput_title,
                xlabel=self.prefill_sequence_len_str,
                ylabel=self.prefill_token_per_second_str,
                x_unit=self.token_units,
                y_unit=self.token_throughput_unit,
                figsize=(15, 6)
            )
            self.save_scatter_plot(
                x_data=prefill_sequence_len,
                y_data=prefill_throughput,
                config=prefill_sequence_len_throughput_config
            )
        else:
            self.logger.warning("[MIE01W000004] There's None in prefill_batch_size, ignore to draw relevant figures.")

    def draw_multi_runs(self, experiment_results):
        '''
        多次实验画的图
        - 直方图:

        - 散点图:
            - token_per_second_vs_avg_batch_size
            - query_per_second_vs_avg_batch_size
        '''
        token_per_second_avg_batch_size_title = self.scatter_plot_title.format(
            self.token_per_second_str,
            self.avg_batch_size_str
        )
        token_per_second_avg_batch_size_config = ScatterPlotConfig(
            filename=f"{token_per_second_avg_batch_size_title}.png",
            save_path=self.save_path,
            title=token_per_second_avg_batch_size_title,
            xlabel=self.avg_batch_size_str,
            ylabel=self.token_per_second_str,
            x_unit=self.batch_size_unit,
            y_unit=self.token_throughput_unit,
            figsize=(15, 6),
            x_key=self.avg_batch_size_str,
            y_key=self.token_per_second_str
        )
        # 保存token_per_second散点图
        self.save_scatter_plot(
            x_data=experiment_results,
            y_data=experiment_results,
            config=token_per_second_avg_batch_size_config
        )

        query_per_second_avg_batch_size_title = self.scatter_plot_title.format(
            self.query_per_second_str,
            self.avg_batch_size_str
        )
        query_per_second_avg_batch_size_config = ScatterPlotConfig(
            filename=f"{query_per_second_avg_batch_size_title}.png",
            save_path=self.save_path,
            title=query_per_second_avg_batch_size_title,
            xlabel=self.avg_batch_size_str,
            ylabel=self.query_per_second_str,
            x_unit=self.batch_size_unit,
            y_unit=self.request_throughput_unit,
            figsize=(15, 6),
            x_key=self.avg_batch_size_str,
            y_key=self.query_per_second_str
        )
        # 保存query_per_second散点图
        self.save_scatter_plot(
            x_data=experiment_results,
            y_data=experiment_results,
            config=query_per_second_avg_batch_size_config
        )

        time_elapsed_throughput_title = self.scatter_plot_title.format(
            self.time_elapsed_str,
            self.query_per_second_str
        )
        time_elapsed_throughput_config = ScatterPlotConfig(
            filename=f"{time_elapsed_throughput_title}.png",
            save_path=self.save_path,
            title=time_elapsed_throughput_title,
            xlabel=self.time_elapsed_str,
            ylabel=self.query_per_second_str,
            x_unit=self.second_unit,
            y_unit=self.request_throughput_unit,
            figsize=(15, 6),
            x_key=self.time_elapsed_str,
            y_key=self.query_per_second_str
        )
        # 保存time_elapsed_throughput散点图
        self.save_scatter_plot(
            x_data=experiment_results,
            y_data=experiment_results,
            config=time_elapsed_throughput_config
        )
