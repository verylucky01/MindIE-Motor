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

import sys
import os

sys.path.append(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)
from mindieclient.python.httpclient import MindIEHTTPClient


def create_client(request_count=1):
    url = "http://127.0.0.1:1025"
    verbose = True
    enable_ssl = False
    ca_certs = "ca.pem"
    key_file = "client.key.pem"
    cert_file = "client.pem"

    # create client
    try:
        if enable_ssl:
            ssl_options = {}
            if ca_certs is not None:
                ssl_options["ca_certs"] = ca_certs
            if key_file is not None:
                ssl_options["keyfile"] = key_file
            if cert_file is not None:
                ssl_options["certfile"] = cert_file
            mindie_client = MindIEHTTPClient(
                url=url,
                verbose=verbose,
                enable_ssl=enable_ssl,
                ssl_options=ssl_options,
                concurrency=request_count,
            )
        else:
            mindie_client = MindIEHTTPClient(
                url=url, verbose=verbose, concurrency=request_count, enable_ssl=enable_ssl,
            )
    except Exception as e:
        raise e
    return mindie_client
