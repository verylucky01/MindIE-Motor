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

import csv

from mindiebenchmark.common.tokenizer import BenchmarkTokenizer
from .base_loader import BaseLoader


class TokenidLoader(BaseLoader):
    def read_file(self, file_path: str):
        with open(self.dataset_path, mode="r", encoding="utf-8") as file:
            csv_reader = csv.reader(file)
            for tokens in csv_reader:
                if self.stop:
                    return
                self._validate_tokenid_range(tokens)
                self.put_data_dict(tokens)

    def get_file_posix(self) -> str:
        return self.CSV_POSIX

    def _validate_tokenid_range(self, tokenid_list: list):
        converted_list = [int(x) for x in tokenid_list]
        try:
            BenchmarkTokenizer.decode(converted_list)
        except RuntimeError as run_error:
            raise RuntimeError("Run tokenizer decode error") from run_error
        except Exception as e:
            raise ValueError(f"validate tokenid range failed and the reason is: {e}") from e
