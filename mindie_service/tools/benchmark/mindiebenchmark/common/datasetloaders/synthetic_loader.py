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

from typing import Dict, List
import json
import pandas as pd

from mindiebenchmark.common.utils import sample_one_value
from mindiebenchmark.common.params_check import check_range
from mindiebenchmark.common.params_check import check_type
from mindiebenchmark.common.params_check import NumberRange
from mindiebenchmark.common.file_utils import PathCheck
from .base_loader import BaseLoader


def _check_keys_equal(got_keys, true_keys, comment):
    for key in got_keys:
        if key not in true_keys:
            raise ValueError(f"{key} is not a valid key for {comment}.")
    if got_keys != true_keys:
        raise ValueError(f"Expect keys {true_keys} for {comment}, but got keys {set(got_keys)}.")


class SyntheticLoader(BaseLoader):

    @staticmethod
    def _check_config_csv(data: pd.DataFrame):
        if data.shape[0] == 0:
            raise ValueError("The `SyntheticConfigPath` file is empty.")
        if data.shape[1] > 2**20:   # 1M
            raise ValueError(f"The number of rows in `SyntheticConfigPath` should be equal or less than {2**20}.")
        if data.shape[1] != 2:
            raise ValueError("CSV must have and only have two columns, which represent the number of input tokens and "
                             "output tokens respectively.")
        if not all(data.applymap(lambda x: isinstance(x, int))):
            raise ValueError("All values should be `int` in `SyntheticConfigPath` .csv file.")
        if not all(data.iloc[:, 0] > 0):
            raise ValueError("The number of input tokens should be greater than 0.")
        if not all(data.iloc[:, 1] > 0):
            raise ValueError("The number of output tokens should be greater than 0")

    @staticmethod
    def _check_config_json(synthetic_config: Dict):
        input_str = "Input"
        output_str = "Output"
        request_count_str = "RequestCount"
        _check_keys_equal(synthetic_config.keys(), {input_str, output_str, request_count_str}, "SyntheticConfig")

        request_count = synthetic_config.get(request_count_str)
        check_type(request_count_str, request_count, (int,))
        check_range(request_count_str, request_count, NumberRange(0, 2**20))

        for key in (input_str, output_str):
            conf = synthetic_config.get(key)
            _check_keys_equal(conf.keys(), {"Method", "Params"}, 'SyntheticConfig["{key}"]')
            method = conf.get("Method")
            params = conf.get("Params")
            uniform_str = "uniform"
            gaussian_str = "gaussian"
            zipf_str = "zipf"
            min_value_str = "MinValue"
            max_value_str = "MaxValue"
            mean_str = "Mean"
            var_str = "Var"
            alpha_str = "Alpha"

            if method == uniform_str:
                _check_keys_equal(params.keys(), {min_value_str, max_value_str}, uniform_str)
            elif method == gaussian_str:
                _check_keys_equal(params.keys(), {mean_str, var_str, min_value_str, max_value_str}, gaussian_str)
            elif method == zipf_str:
                _check_keys_equal(params.keys(), {alpha_str, min_value_str, max_value_str}, zipf_str)
            else:
                raise ValueError(f'Method should be one of {{{uniform_str, gaussian_str, zipf_str}}}, '
                                 f'but got {method}.')
            min_float32_value = -3.0e38
            max_float32_value = 3.0e38
            for param_name, param_value in params.items():
                desc_name = key + " " + param_name
                if param_name in (min_value_str, max_value_str):
                    check_type(desc_name, param_value, types=(int,))
                    if key == input_str:
                        check_range(desc_name, param_value, NumberRange(1, 2**20))    # 2**20 = 1M
                    elif key == output_str:
                        check_range(desc_name, param_value, NumberRange(1, 2**20))
                elif param_name == mean_str:
                    check_type(desc_name, param_value, types=(int, float))
                    check_range(desc_name, param_value, NumberRange(min_float32_value, max_float32_value))
                elif param_name == var_str:
                    check_type(desc_name, param_value, types=(int, float))
                    check_range(desc_name, param_value, NumberRange(0, max_float32_value))
                elif param_name == alpha_str:
                    check_type(desc_name, param_value, types=(int, float))
                    check_range(desc_name, param_value, NumberRange(1.0, 10.0, lower_inclusive=False))
            min_value = params.get(min_value_str)
            max_value = params.get(max_value_str)
            if min_value > max_value:
                raise ValueError(f'MinValue should less than MaxValue, '
                                 f'but got MinValue is {min_value}, and MaxValue is {max_value}.')

    def read_line(self, line: List[int]) -> Dict:
        """Get a data dict according to line.

        Args:
            line (List[int]): Input line should be a list with 2 integral elements, it represents the number of input
                token and output token respectively.

        Returns:
            A data dictionary which contains 3 keys: "id", "data", "max_new_tokens".
        """
        if not hasattr(line, '__len__') or len(line) != 2:
            raise ValueError("Input line should be a list with 2 integral elements.")
        default_str = "A"
        num_input_token, num_expect_generated_tokens = line
        data = " ".join([default_str] * num_input_token)
        return {
            "id": f"synthetic/{self.id_count}",
            "data": data,
            "num_expect_generated_tokens": num_expect_generated_tokens
        }

    def read_file(self, file_path: str):
        ret, infos = PathCheck.check_path_full(file_path)
        if not ret:
            raise ValueError(f"{infos} Got path: {file_path}")

        suffix = file_path.split(".")[-1]
        num_input_output_tokens = []
        if suffix.lower() == "csv":
            num_input_output_tokens = self._read_csv_file(file_path)
        elif suffix.lower() == "json":
            num_input_output_tokens = self._read_json_file(file_path)
        else:
            raise ValueError(f"`SyntheticConfigPath` should be `.csv` or `.json` file, but got {suffix.lower()}")
        for line in num_input_output_tokens:
            data = self.read_line(line)
            if not self.stop:
                self.put_data_dict(data)

    def get_file_list(self) -> List[str]:
        return [self.synthetic_config_path]

    def _read_csv_file(self, config_path):
        df = pd.read_csv(config_path)
        self._check_config_csv(df)
        num_input_output_tokens = df.values.tolist()
        return num_input_output_tokens

    def _read_json_file(self, config_path):
        try:
            with open(config_path, mode="r", encoding="utf-8") as file:
                config = json.load(file)
        except (FileNotFoundError, json.JSONDecodeError) as e:
            raise ValueError("Failed to load JSON config from `SyntheticConfigPath` file.") from e
        self._check_config_json(config)
        request_count = config.get("RequestCount")
        input_method = config["Input"]["Method"]
        input_params = config["Input"]["Params"]
        output_method = config["Output"]["Method"]
        output_params = config["Output"]["Params"]
        num_input_output_tokens = [[sample_one_value(input_method, input_params),
                                        sample_one_value(output_method, output_params)]
                                    for _ in range(request_count)]
        return num_input_output_tokens
