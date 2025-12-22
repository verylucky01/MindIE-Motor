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

import re
import os 
import signal
from typing import Any, Tuple
from dataclasses import dataclass
from .logging import Logger


@dataclass
class NumberRange:
    lower: tuple[int, float] = None
    upper: tuple[int, float] = None
    lower_inclusive: bool = True
    upper_inclusive: bool = True


def check_type_and_range(name, value, types, value_range):
    """
    Validate a parameter's type and range.

    Args:
        name: The name of the variable (str).
        value: The value of the variable (int, float, or bool).
        types: A tuple of allowed types (e.g., (int, float)).
        value_range: A string defining the allowed range (e.g., "(0.0, +inf)").
                        Parentheses () mean exclusive, brackets [] mean inclusive.
    Raises:
        ValueError: If the value's type or range is invalid.
    """
    # Validate type
    try:
        if not isinstance(value, types):
            raise ValueError(f"{name} check failed: expect one of {types}, but got {type(value)}")
    except ValueError as e:
        log = Logger().logger
        log.error(f"Exception:{e}")
        os.kill(os.getpid(), signal.SIGKILL)
    if not value_range:
        return

    # Parse value_range
    match = re.match(
        r"^([\[\(])\s*(-?inf|\+?inf|[-+]?\d*\.?\d+)\s*,\s*(-?inf|\+?inf|[-+]?\d*\.?\d+)\s*([\]\)])$",
        value_range)
    if not match:
        raise ValueError(f"{name} check failed: invalid range format '{value_range}'")
    left_bracket, lower_bound, upper_bound, right_bracket = match.groups()

    # Convert bounds to float
    lower_bound = float("-inf") if lower_bound == "-inf" else float(lower_bound)
    upper_bound = float("inf") if upper_bound == "+inf" else float(upper_bound)

    # Check value within range
    in_range = (
        (lower_bound < value if left_bracket == '(' else lower_bound <= value) and
        (value < upper_bound if right_bracket == ')' else value <= upper_bound)
    )

    if not in_range:
        raise ValueError(
            f"{name} check failed: value {value} not in range {value_range}"
        )


def check_type(name: str, value: Any, types: Tuple):
    if not isinstance(value, types):
        raise ValueError(f"Parameter {name} should have type {types}, but got {type(value)}.")


def check_range(name: str, value: Any, param: NumberRange):
    lower, upper = param.lower, param.upper
    lower_inclusive, upper_inclusive = param.lower_inclusive, param.upper_inclusive
    # 构造区间的字符串表示
    lower_bound = '[' if lower_inclusive else '('
    upper_bound = ']' if upper_inclusive else ')'
    lower_str = str(lower) if lower is not None else '-inf'
    upper_str = str(upper) if upper is not None else '+inf'

    # 构造完整区间表示字符串
    interval_str = f"{lower_bound}{lower_str}, {upper_str}{upper_bound}"

    # 检查下限
    if lower is not None:
        lt = (lower_inclusive and value < lower)
        le = (not lower_inclusive and value <= lower)
        if le or lt:
            raise ValueError(f"Parameter {name} is {value}, not within the required range {interval_str}")
    # 检查上限
    if upper is not None:
        gt = (upper_inclusive and value > upper)
        ge = (not upper_inclusive and value >= upper)
        if gt or ge:
            raise ValueError(f"Parameter {name} is {value}, not within the required range {interval_str}")
