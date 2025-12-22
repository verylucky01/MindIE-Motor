# MindIE-Client用户指南
## 1 MindIE-Client介绍
MindIE-Client是MindIE-Motor的模块之一。在启动MindIE-Server Endpoint服务后，MindIE-Client可以使用HTTP协议对它发送请求。MindIE-Client提供了多种功能的接口，包括：模型推理、请求管理和服务状态查询，用户调用接口即可实现与MindIE-Server通信，提升了易用性。

其中模型推理部分支持：同步推理（token_id to token_id）、异步推理（token_id to token_id）、全量文本推理（text to text）、流式文本推理（text to text）

请求管理部分支持：提前终止推理请求、统计slot数量

服务状态查询部分支持：查询Server和Model的状态和元数据、查询Model配置

## 2 MindIE-Client部署

1.提前安装CANN、atb、atb_model和torch_npu等依赖环境

2.下载`Ascend-mindie-<version>_linux_aarch64.run`，安装run包，即会自动安装mindieclient的whl包，若安装失败，也可以进入`/usr/local/Ascend/mindie/<version>/mindie-service/bin`目录，手动安装mindieclient的whl包。

3.进入`/usr/local/Ascend/mindie/<version>/mindie-service/bin`目录，部署MindIE-Server Endpoint服务。即可import使用mindieclient相关接口（如需启动https认证，需要提供CA证书、客户端证书和客户端密钥用于服务器认证）

## 3 MindIE-Client样例代码
功能接口调用可参考MindIE-Motor/mindieclient/test目录下的测试用例。我们从mindieclient.python.httpclient中导出需要的模块，调用接口，发送请求。以同步推理接口为例：
```
import argparse
import sys
import os
import numpy as np

from mindieclient.python.httpclient import MindIEHTTPClient, Input, RequestedOutput

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-u",
        "--url",
        required=False,
        default="127.0.0.1:1025",
        help="MindIE-Server URL.",
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
        default=True,
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
                ssl=True,
                ssl_options=ssl_options,
                concurrency=request_count,
            )
        else:
            mindie_client = MindIEHTTPClient(
                url=args.url, verbose=args.verbose, concurrency=request_count
            )
    except Exception:
        print("Client Creation failed!")
        sys.exit(1)

    # create input and requested output
    inputs = []
    outputs = []
    input_data = np.arange(start=0, stop=16, dtype=np.uint32)
    input_data = np.expand_dims(input_data, axis=0)
    inputs.append(Input("INPUT0", [1, 16], "UINT32"))
    inputs[0].initialize_data(input_data)

    outputs.append(RequestedOutput("OUTPUT0"))
    # apply model inference
    model_name = "llama_65b"
    results = mindie_client.infer(
        model_name,
        inputs,
        outputs=outputs,
    )
    print(results.get_response())

    output_data = results.retrieve_output_name_to_numpy("OUTPUT0")
    print("input_data: ", input_data)
    print("output_data: ", output_data)
```