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
import numpy as np

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from httpclient import Input, RequestedOutput
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

    # create input and requested output
    inputs = []
    outputs = []
    input_data = np.arange(start=0, stop=16, dtype=np.uint32)
    input_data = np.expand_dims(input_data, axis=0)
    input_shape = np.array([1, 16], dtype=np.uint32)
    inputs.append(Input("INPUT0", input_shape, "UINT32"))
    inputs[0].initialize_data(input_data)

    outputs.append(RequestedOutput("OUTPUT0"))
    # apply model inference
    model_name = "llama_65b"
    results = mindie_client.infer(
        model_name,
        inputs,
        outputs=outputs,
    )
    logger.info(results.get_response())

    output_data = results.retrieve_output_name_to_numpy("OUTPUT0")
    logger.info("input_data: %s", np.array2string(input_data))
    logger.info("output_data: %s", np.array2string(output_data))
