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

import argparse
import sys
import os

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from httpclient import MindIEHTTPClient


def create_client(request_count=1):
    # get argument
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-u",
        "--url",
        required=False,
        default="http://127.0.0.1:1025",
        help="MindIE-Server URL.",
    )
    parser.add_argument(
        "-mu",
        "--management_url",
        required=False,
        default="http://127.0.0.2:1026",
        help="MindIE-Server management URL.",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        required=False,
        default=True,
        help="Enable detailed information output.",
    )
    parser.add_argument(
        "-s",
        "--ssl",
        action="store_true",
        required=False,
        default=False,
        help="Enable encrypted link with https",
    )
    parser.add_argument(
        "-ca",
        "--ca_certs",
        required=False,
        default="ca.pem",
        help="Provide https ca certificate.",
    )
    parser.add_argument(
        "-key",
        "--key_file",
        required=False,
        default="client.key.pem",
        help="Provide https client certificate.",
    )
    parser.add_argument(
        "-cert",
        "--cert_file",
        required=False,
        default="client.pem",
        help="Provide https client keyfile.",
    )
    args = parser.parse_args()

    # create client
    try:
        if args.ssl:
            ssl_options = {}
            if args.ca_certs is not None:
                ssl_options["ca_certs"] = args.ca_certs
            if args.key_file is not None:
                ssl_options["keyfile"] = args.key_file
            if args.cert_file is not None:
                ssl_options["certfile"] = args.cert_file
            mindie_client = MindIEHTTPClient(
                url=args.url,
                verbose=args.verbose,
                enable_ssl=True,
                ssl_options=ssl_options,
                concurrency=request_count,
            )
        else:
            mindie_client = MindIEHTTPClient(
                url=args.url, verbose=args.verbose, enable_ssl=False,
                concurrency=request_count, management_url=args.management_url
            )
    except Exception as e:
        raise e
    return mindie_client
