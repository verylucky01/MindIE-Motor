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

from .base_loader import BaseLoader


class CocotestLoader(BaseLoader):
    TestType = "client"
    TGI_PREFIX = "![]("
    TGI_POSIX = ")Generate the caption in English with grounding:"

    @classmethod
    def validate_dataset_args(cls, dataset_type: str, command_line_args: dict):
        super().validate_dataset_args(dataset_type, command_line_args)
        cls.TestType = command_line_args.get("TestType")
        support_type = ["client", "tgi_client"]
        if cls.TestType not in support_type:
            raise ValueError(f"argument --TestType: {cls.TestType} invalid, "
                             f"{dataset_type} can only run in {support_type} mode")

    def read_file(self, file_path: str):
        if self.TestType == "tgi_client":
            data = self.TGI_PREFIX + file_path + self.TGI_POSIX
        else:
            data = [
                {"type": "text", "text": "What is in this image?"},
                {"type": "image_url", "image_url": file_path}
            ]
        self.put_data_dict(data)

    def get_file_posix(self) -> str:
        return self.JPG_POSIX
