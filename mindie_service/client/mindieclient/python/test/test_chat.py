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

import sys
import os

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common.logging import Log
from utils import create_client

logger = Log(__name__).getlog()

if __name__ == "__main__":
    # get argument and create client
    try:
        mindie_client = create_client()
    except Exception:
        logger.exception("Client Creation failed!")
        sys.exit(1)

    # create input
    prompt = "My name is Olivier and I"
    model_name = "llama_65b"
    parameters = {
        "best_of": 1,
        "n": 1,
        "logprobs": True,
        "top_logprobs": 2,
        "stream": False
    }
    # apply model inference
    results = mindie_client.generate_chat(
        model_name,
        prompt,
        request_id="001",
        parameters=parameters
    )
    logger.info("response: %s", results)
