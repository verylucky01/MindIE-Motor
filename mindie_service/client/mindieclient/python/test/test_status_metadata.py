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
import json

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

    model_name = "llama_65b"
    if mindie_client.is_server_live():
        logger.info("The server is alive!")
    else:
        logger.error("The server is not alive!")
        sys.exit(1)
    if mindie_client.is_server_ready():
        logger.info("The server is ready!")
    else:
        logger.error("The server is not ready!")
        sys.exit(1)
    if mindie_client.is_model_ready(model_name):
        logger.info("The model is ready!")
    else:
        logger.error("The model is not ready!")
        sys.exit(1)
    server_metadata_dict = mindie_client.get_server_metadata()
    logger.info("get_server_metadata: %s", json.dumps(server_metadata_dict))
    model_metadata_dict = mindie_client.get_model_metadata(model_name)
    logger.info("get_model_metadata: %s", json.dumps(model_metadata_dict))
    model_config_dict = mindie_client.get_model_config(model_name)
    logger.info("get_model_config: %s", json.dumps(model_config_dict))

    # get slots
    result = mindie_client.get_slot_count(model_name)
    logger.info("gets_slot_count: %s", json.dumps(result.get_response()))
