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

import os
import sys
import re
import stat
from dataclasses import dataclass
from urllib.parse import quote, urlparse
import rapidjson as json

# append mindie service path
python_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if not os.path.exists(python_path):
    raise ValueError(f"Path does not exist: {python_path}")
if not os.path.isdir(python_path):
    raise ValueError(f"Path is not a directory: {python_path}")
mindieservice_path = os.path.dirname(os.path.dirname(python_path))
if not os.path.exists(mindieservice_path):
    raise ValueError(f"Path does not exist: {mindieservice_path}")
if not os.path.isdir(mindieservice_path):
    raise ValueError(f"Path is not a directory: {mindieservice_path}")
sys.path.append(mindieservice_path)
from mindieclient.python.common.logging import Log
from mindieclient.python.common.file_utils import PathCheck


@dataclass
class NumberRange:
    name: str
    value: tuple[int, float]
    lower: tuple[int, float] = None
    upper: tuple[int, float] = None
    lower_inclusive: bool = True
    upper_inclusive: bool = True


def raise_error(message):
    logger = Log(__name__).getlog()
    logger.error(message)
    raise MindIEClientException(message=message) from None


def raise_warn(message):
    logger = Log(__name__).getlog()
    logger.warning(message)


SSL_MUST_KEYS = ["ca_certs", "certfile", "keyfile"]
CRL_FILE = "crlfile"


def _check_variable_type(variable_map: {}, types: ()):
    for name, val in variable_map.items():
        if not isinstance(val, types):
            raise_error(f"The {name}'s type should be {types} but got {type(val)}")


def _check_invalid_url(url, enable_ssl):
    if not isinstance(url, str):
        raise_error("[MIE02E000002] URL version should be a string!")
    parse_res = urlparse(url)
    if enable_ssl and parse_res.scheme != "https":
        raise_error("[MIE02E000002] The scheme should be https when ssl enables!")
    if not enable_ssl and parse_res.scheme != "http":
        raise_error("[MIE02E000002] The scheme should be http when ssl disables!")
    if not parse_res.netloc:
        raise_error("[MIE02E000002] URL should contain netloc!")


def _check_directory_permissions(cur_path):
    cur_stat = os.stat(cur_path)
    cur_mode = stat.S_IMODE(cur_stat.st_mode)
    if cur_mode != 0o700:
        raise_error("The permission of ssl directory should be 700")


def _check_invalid_ssl_filesize(ssl_options):
    def check_size(path: str):
        size = os.path.getsize(path)
        if size > max_size:
            raise_error(f"SSL file should not exceed 10MB!")

    max_size = 10 * 1024 * 1024  # 最大文件大小为10MB
    for ssl_key in SSL_MUST_KEYS:
        check_size(ssl_options[ssl_key])


def _check_invalid_ssl_path(ssl_options):
    def check_single(key: str, path: str):
        is_path_valid, infos = PathCheck.check_cert_path(path)
        if not is_path_valid:
            logger = Log(__name__).getlog()
            logger.error(f'[MIE02E000000] {infos} Please check ssl path.')
            raise_error(f"Enum {key} path is invalid")
        _check_directory_permissions(os.path.dirname(path))

    if not isinstance(ssl_options, dict):
        raise_error("ssl_options should be a dict!")
    for ssl_key in SSL_MUST_KEYS:
        if ssl_key not in ssl_options.keys():
            raise_error(f"{ssl_key} should be provided when ssl enables!")
        check_single(ssl_key, ssl_options[ssl_key])


def _check_ca_cert(ssl_options, plain_text):
    server_dir = os.getenv('MIES_INSTALL_PATH')
    if server_dir is None:
        raise_error("[MIE02E000002] Not Found $MIES_INSTALL_PATH, please source mindie set_env.sh firstly.")
    cert_util_path = os.path.join(server_dir, "scripts")
    ret, infos = PathCheck.check_path_full(cert_util_path)
    if not ret:
        raise_error(f"Cert utils path is not found, {infos}")
    if cert_util_path not in sys.path:
        sys.path.append(cert_util_path)

    from utils import CertUtil

    ca = "ca_certs"
    cert = "certfile"
    key = "keyfile"

    if not CertUtil.validate_ca_certs(ssl_options[ca]):
        raise_error("CA check failed.")
    if CRL_FILE in ssl_options.keys():
        if not CertUtil.validate_ca_crl(ssl_options[ca], ssl_options[CRL_FILE]):
            raise_error("CRL check failed")
    if not CertUtil.validate_cert_and_key(ssl_options[cert], ssl_options[key], plain_text):
        raise_error("Client Cert check failed.")


class MindIEClientException(Exception):
    def __init__(self, message):
        super().__init__()
        if len(message) == 0 or len(message) > 512000:
            raise_error("The length of message should be in range[1, 512000], \
                but got {}".format(len(message)))
        self._error_message = message

    def get_message(self):
        return self._error_message


def generate_request_body(
    inputs,
    request_id,
    parameters,
    outputs=None,
    is_generate=False,
):
    request_dict = {}
    request_dict["id"] = request_id
    if is_generate:
        request_dict["text_input"] = inputs
    else:
        request_dict["inputs"] = []
        for cur_input in inputs:
            request_dict["inputs"].append(cur_input.get_input_tensor())

        request_dict["outputs"] = []
        for cur_output in outputs:
            request_dict["outputs"].append(cur_output.get_output_tensor())

    if parameters != {}:
        request_dict["parameters"] = parameters

    request_body = json.dumps(request_dict)
    return request_body.encode()


def generate_request_body_token(
    token,
    is_stream,
    parameters
):
    request_dict = {}
    request_dict["input_id"] = token[0].tolist()
    request_dict["stream"] = is_stream

    if parameters != {}:
        request_dict["parameters"] = parameters

    request_body = json.dumps(request_dict)
    return request_body.encode()


def get_infer_request_uri(model_name, model_version):
    if not isinstance(model_version, str):
        raise_error("Model version should be a string!")
    if model_version != "":
        request_uri = "/v2/models/{}/versions/{}/infer".format(
            quote(model_name), model_version
        )
    else:
        request_uri = "/v2/models/{}/infer".format(quote(model_name))
    return request_uri


def check_response(response):
    if response.status == 200:
        return
    try:
        json_content = response.data.decode("utf-8")
        response_dict = json.loads(json_content)
        if not response_dict.get("error"):
            error = MindIEClientException(
                message=f"Error status code: {response.status_code}"
            )
        else:
            error = MindIEClientException(message=response_dict["error"])
    except ValueError as value_error:
        error = MindIEClientException(
            message=f"An exception occurred in client while decoding the response: {value_error}"
        )
    except KeyError as key_error:
        error = MindIEClientException(
            message=f"An exception occurred in client, lacking key: {key_error} in the response"
        )
    except Exception as e:
        error = MindIEClientException(
            message=f"An exception occurred in client while decoding the response: {e}"
        )
    logger = Log(__name__).getlog()
    logger.error("[MIE02W00006] Get response failed : %s", error.get_message())
    raise error


def validate_model_string(model_name, model_version=""):
    if len(model_name) > 1000:
        raise_error("The length of model name should be no more than 1000!")
    if len(model_version) > 1000:
        raise_error("The length of model version should be no more than 1000!")
    pattern = r"^[a-zA-Z_0-9.\-\\/]+$"
    if not isinstance(model_name, str) or not re.match(pattern, model_name):
        raise_error("Model name should be a string and valid!")
    if not isinstance(model_version, str) or (
        model_version != "" and not re.match(pattern, model_version)
    ):
        raise_error("Model version should be a string and valid!")


def check_password(password):
    min_pass_len = 8
    if len(password) < min_pass_len:
        raise_error("Password length must be at least 8 characters.")
    max_pass_len = 128
    if len(password) > max_pass_len:
        raise_error(
            "Password length must be less than 128 characters. "
            "For better security, we recommend keeping it under 64 characters."
        )

    # uppercase, lowercase, digit, symbol
    types = [0, 0, 0, 0]
    special_characters = set("~!@#$%^&*()-_=+\\|[{}];:'\",<.>/? ")
    for char in password:
        if char.isupper():
            types[0] = 1
        elif char.islower():
            types[1] = 1
        elif char.isdigit():
            types[2] = 1
        elif char in special_characters:
            types[3] = 1
        else:
            raise_error("Password check failed.")

    # 检查是否至少包括两种类型的字符
    if sum(types) < 2:
        raise_error("Password check failed.")


def check_type(name, value, types):
    if not isinstance(value, types):
        raise_error(f"Parameter {name} should have type {types}, but got {type(value)}.")


def check_range(param: NumberRange):
    name, value = param.name, param.value
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
            raise_error(f"Parameter {name} is {value}, not within the required range {interval_str}")
    # 检查上限
    if upper is not None:
        gt = (upper_inclusive and value > upper)
        ge = (not upper_inclusive and value >= upper)
        if gt or ge:
            raise_error(f"Parameter {name} is {value}, not within the required range {interval_str}")


def check_sampling_params(params):
    float_min = -float('inf')
    float_max = float('inf')
    int32_min = -2147483647
    int32_max = 2147483647
    int64_min = -18446744073709551615
    int64_max = 18446744073709551615
    seed_min = 1
    if not params:
        return False, "Sampling parameter is empty, default value will be used"
    for key, value in params.items():
        if key == "temperature":
            check_type(key, value, (int, float))
            check_range(NumberRange(name=key, value=value, lower=float_min, upper=float_max))
        elif key == "top_k":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "top_p":
            check_type(key, value, (int, float))
            check_range(NumberRange(name=key, value=value, lower=float_min, upper=float_max))
        elif key == "typical_p":
            check_type(key, value, (int, float))
            check_range(NumberRange(name=key, value=value, lower=float_min, upper=float_max))
        elif key == "seed":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=seed_min, upper=int64_max))
        elif key == "repetition_penalty":
            check_type(key, value, (int, float))
            check_range(NumberRange(name=key, value=value, lower=float_min, upper=float_max))
        elif key == "watermark":
            check_type(key, value, (bool,))
        elif key == "truncate":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "details":
            check_type(key, value, (bool,))
        elif key == "stream":
            check_type(key, value, (bool,))
        elif key == "do_sample":
            check_type(key, value, (bool,))
        elif key == "max_tokens":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "max_new_tokens":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "presence_penalty":
            check_type(key, value, (int, float))
            check_range(NumberRange(name=key, value=value, lower=float_min, upper=float_max))
        elif key == "frequency_penalty":
            check_type(key, value, (int, float))
            check_range(NumberRange(name=key, value=value, lower=float_min, upper=float_max))
        elif key == "include_stop_str_in_output":
            check_type(key, value, (bool,))
        elif key == "skip_special_tokens":
            check_type(key, value, (bool,))
        elif key == "ignore_eos":
            check_type(key, value, (bool,))
        elif key == "priority":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "timeout":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int64_min, upper=int64_max))
        elif key == "firstTokenCost":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int64_min, upper=int64_max))
        elif key == "decodeTime":
            check_type(key, value, (list,))
        elif key == "best_of":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "perf_stat":
            check_type(key, value, (bool,))
        elif key == "n":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        elif key == "use_beam_search":
            check_type(key, value, (bool,))
        elif key == "logprobs":
            check_type(key, value, (int, bool))
        elif key == "top_logprobs":
            check_type(key, value, (int,))
            check_range(NumberRange(name=key, value=value, lower=int32_min, upper=int32_max))
        else:
            raise_warn(f"Maybe parameter {key} is not supported, please check it.")
    return True, ""
