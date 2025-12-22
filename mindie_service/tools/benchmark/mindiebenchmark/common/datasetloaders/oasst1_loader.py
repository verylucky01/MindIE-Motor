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

import os.path
import pyarrow.parquet as pq

from .base_loader import BaseLoader


class Oasst1Loader(BaseLoader):
    def read_file(self, file_path: str):
        data_dict = pq.read_table(file_path).to_pandas()
        data_dict = data_dict[data_dict["lang"] == "zh"]
        ques_list = data_dict["text"].to_list()
        file_name = os.path.basename(file_path).split("-")[0]
        for i, ques in enumerate(ques_list):
            if self.stop:
                return
            key = file_name + str(i)
            self.put_data_dict({"id": key, "data": ques})

    def get_file_posix(self) -> str:
        return self.PARQUET_POSIX
