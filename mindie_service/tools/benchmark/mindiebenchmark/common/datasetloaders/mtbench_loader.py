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

from .base_loader import JsonlLoader


class MtbenchLoader(JsonlLoader):
    @classmethod
    def validate_dataset_args(cls, dataset_type: str, command_line_args: dict):
        super().validate_dataset_args(dataset_type, command_line_args)
        # 目前 mtbench 数据集仅支持 client 模式
        client_mode_str = "client"
        test_type = command_line_args.get("TestType")
        if test_type != client_mode_str:
            raise ValueError(f"argument --TestType: {test_type} invalid, "
                             f"{dataset_type} can only run in {client_mode_str} mode")
        # 目前 mtbench 数据集不支持流控
        request_rate = command_line_args.get("RequestRate")
        if request_rate:
            raise ValueError(f"{dataset_type} doesn't support setting RequestRate")

    def read_line(self, line):
        return {
            "id": line["question_id"],
            "data": line["turns"],
        }
