#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import threading
import types


class ThreadSafeFactory:
    @staticmethod
    def make_threadsafe_instance(type_name, *instance_args, **instance_kwargs):
        class ThreadSafeCls(type_name):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self._lock = threading.Lock()
                self._wrappers = {}

            def __getattr__(self, name):
                if name in self._wrappers:
                    return self._wrappers[name]
                with self._lock:
                    attr = getattr(super(), name)
                if callable(attr):
                    def wrapper(*args, **kwargs):
                        with self._lock:
                            return attr(*args, **kwargs)
                    self._wrappers[name] = wrapper
                    return wrapper
                else:
                    return attr

        return ThreadSafeCls(*instance_args, **instance_kwargs)