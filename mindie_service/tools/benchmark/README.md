# MindIE Benchmark
## 功能介绍：
服务化benchmark工具是通过部署昇腾服务化配套包后，通过调用终端命令的方式测试大语言模型的在配置不同配置参数下的在NPU卡上的推理端到端性能和精度，并通过表格的形式展示模型在各个阶段的推理耗时（FirstTokenTime,DecodeTime），以及所有的推理请求结果的平均、最小、最大、75分位和99分位等概率统计值，最后将计算结果保存到本地csv文件中, 并生成可视化分析结果。

### 目前支持两种不同的后端推理模式：
### Client模式
Benchmark支持调用MindIE Client接口的方式，与 endpoint 进行通信并完成测试。目前支持的推理模式包括：
1. 文本模式：此模式输入为文本形式，接收的数据也为文本形式。该模式下支持全量文本生成及流式文本生成两种, 调用MindIE-Client的.generate()和.generate_stream()接口。

### 北向接口模式
Benchmark支持通过直接调用北向接口提供的InferenceEngine python API进行流式推理。
1. 支持token id到token id异步推理；
2. 文本到文本的异步推理；

## 1.环境部署：

    硬件环境: Atlas 800I A2
    软件环境：python3.10

### 1.1 依赖软件包：
#### 1.1.1 HDK驱动固件包 （driver + firmware包）:
下载： \
    Ascend-hdk-910b-npu-driver_\<version>\_linux-aarch64.run \
    Ascend-hdk-910b-npu-firmware_\<version>.run

安装命令:
```shell
Ascend-hdk-910b-npu-driver_<version>_linux-aarch64.run --full

Ascend-hdk-910b-npu-firmware_<version>.run --full

# 重装完检查是否安装成功，查看驱动版本信息
npu-smi info

# 重装驱动后需要重启服务器:
reboot
```
#### 1.1.2 CANN （toolkit + kernel包 + nnal包）:
下载： \
    Ascend-cann-toolkit_\<version>\_linux-aarch64.run \
    Ascend-cann-nnal_\<version>\_linux-aarch64.run \
    Ascend-cann-kernels-910b_\<version>\_linux.run

安装命令:
```shell
Ascend-cann-toolkit_<version>_linux-aarch64.run --install

Ascend-cann-nnal_<version>_linux-aarch64.run --install

Ascend-cann-kernels-910b_<version>_linux.run --install
```

#### 1.1.3 ATB加速库包:

ATB加速库包在cann的nnal包里面，安装完cann的nnal包之后，默认路径在/usr/local/Ascend/nnal/atb路径下

#### 1.1.4 ATB_Models模型库
下载： \
    Ascend-mindie-atb-models_\<version\>_linux-aarch64_torch2.1.0-abi0.tar.gz

安装命令:
```shell
# 解压
mkdir -p /usr/local/Ascend/atb_models
tar -zxf Ascend-mindie-atb-models_<version>_linux-aarch64_torch2.1.0-abi0.tar.gz -C /usr/local/Ascend/atb_models

cd /usr/local/Ascend/atb_models

pip3 --install atb_llm-<version>-py3-none-any.whl
```

#### 1.1.5 PTA (torch + torch_npu):
下载： \
    torch-2.1.0-cp310-cp310-manylinux_2_17_aarch64.manylinux2014_aarch64.whl \
    pytorch_\<version\>_py310.tar.gz

安装命令:
```shell
# 安装 torch-2.1.0
pip install torch-2.1.0-cp310-cp310-manylinux_2_17_aarch64.manylinux2014_aarch64.whl
# 解压
# 安装 torch_npu-2.1.0
tar -zxf pytorch_<version>_py310.tar.gz
pip3 --install apex-0.1_ascend_<version>-cp310-cp310-linux_aarch64.whl
pip3 --install torch_npu-2.1.0.<version>-cp310-cp310-manylinux_2_17_aarch64.manylinux2014_aarch64.whl
```

#### 1.1.6 MindIE包:
下载： \
    Ascend-mindie_\<version\>_linux-aarch64.run \
mindie默认安装路径为/usr/local/Ascend/mindie/。 \
其他相关python依赖可参考MindIE安装指南--安装开发环境--安装依赖

安装命令:
```shell
# 安装mindie包
./Ascend-mindie_<version>_linux-aarch64.run --install

```

### 1.2 环境变量设置
source 环境变量：

```shell
source /usr/local/Ascend/ascend-toolkit/set_env.sh        ## CANN
source /usr/local/Ascend/nnal/atb/set_env.sh                   ## atb
source /usr/local/Ascend/atb_models/set_env.sh  ## atb_models
source /usr/local/Ascend/mindie/set_env.sh   ## mindie
```

## 2. Benchmark调用命令
### 2.1 模型配置文件
/usr/local/Ascend/mindie/\<version\>/mindie-service/conf/config.json (mindie默认安装路径是/usr/local/Ascend/mindie/)
```shell
# 关键参数介绍修改文件：
"npuMemSize" : 40  # 推理过程npu计算资源，-1时代表自动申请NPU内存

"modelName" : "llama_65b" # 模型名

"modelWeightPath" : "/data/atb_testdata/weights/llama1-65b-safetensors" # 模型权重路径
```

### 2.2 北向接口推理模式
布尔值参数不传参代表该参数为False
```shell
# 北向接口模式
SMPL_PARAM="{\"temperature\":0.5,\"top_k\":10,\"top_p\":0.95,\"typical_p\":1.0,\"seed\":1,\"repetition_penalty\":1.03,\"watermark\":true,\"truncate\":10}"

benchmark \
--DatasetPath "/path/to/dataset/GSM8K" \
--DatasetType "gsm8k" \
--ModelName "llama_65b" \
--ModelPath "/data/atb_testdata/weights/llama1-65b-safetensors" \
--TestType engine \
--Concurrency 3 \
--Tokenizer True \
--DoSampling True \
--TestAccuracy True \
--SamplingParams=$SMPL_PARAM

```

### 2.3 client推理模式
布尔值参数不传参代表该参数为False

**前置依赖**：client模式运行前需要在服务端部署 `mindie-motor` 环境，MindIE-Server部署参考 Mindie-Motor开发指南--配置MindIE Server

支持开启/关闭 `do_sample` 两种模式，在开启 `do_sample` 时，需要配合 `--SamplingParams` 传入采样参数，否则使用如下默认采样参数：
```python
DEFAULT_SMPL_PARAM = {
    "temperature": 0.5,
    "top_k": 10,
    "top_p": 0.9,
    "typical_p": 0.9,
    "seed": 1,
    "repetition_penalty": 1,
    "watermark": True,
    "truncate": 5
}
```

使用 `do_sample` 模式时运行样例：

```shell
SMPL_PARAM="{\"temperature\":0.5,\"top_k\":10,\"top_p\":0.9,\"typical_p\":0.9,\"seed\":1234,\"repetition_penalty\":1,\"watermark\":true,\"truncate\":10}"

benchmark --DatasetPath "/path/to/dataset/GSM8K" --DatasetType "gsm8k" \
  --ModelPath "/path/to/your/model" --TestType client --Http https://127.0.0.1:1025 --Concurrency 16 \
  --ModelName llama_65b --TaskKind stream --DoSampling True --SamplingParams $SMPL_PARAM
```

关闭 `do_sample` 模式时，文本流式推理模式：
```shell
benchmark --DatasetPath "/path/to/dataset/GSM8K" \
  --DatasetType "gsm8k" --ModelPath "/path/to/your/model" --TestType client --Http https://127.0.0.1:1025 \
  --Concurrency 1 --TestAccuracy True --ModelName llama_65b --TaskKind stream
```

关闭 `do_sample` 模式时，文本非流式推理模式：
```shell
benchmark --DatasetPath "/path/to/dataset/GSM8K" \
  --DatasetType "gsm8k" --ModelPath "/path/to/your/model" --TestType client --Http https://127.0.0.1:1025 \
  --Concurrency 1 --TestAccuracy True --ModelName llama_65b --TaskKind text
```

### client 模式下部分命令行参数要求

- `--ModelName <target-model>` 提供模型名字，必选字段，需要Mindie-Server的配置文件中modelName参数保持一致，用于client-sdk拼接得到endpoint对应的uri `/v2/models/<model-name>/{generate,generate_stream}`
- `--ModelVersion` 模型版本，可选，在endpoint部署同一模型（指model name相同）的多个版本时需要提供，此时，client-sdk对应拼接的uri地址为 `/v2/models/<ModelName>/versions/<ModelVerson>/{generate,generate_stream}`
- `--TaskKind` client推理模式，默认为stream
- `--Concurrency` 并发数，限制同时发起的连接数

## 3. Benchmark可视化分析

Benchmark支持输出：

### MindIE-Server组件的性能耗时随时间的分布：
其中X轴是实验过程中MindIE-Server组件各个性能耗时（队列等待时间/后处理时间/tokenizer时间/detokenizer时间），Y轴是各个性能耗时的出现频次

下图为在RequestRate设置为100的情况下，队列等待时间的频次分布
![Frequency_Histogram_of_Queue_Wait_Time_100](docs/Frequency_Histogram_of_Queue_Wait_Time_100.png)

### MindIE-Server组件的调度信息随时间的分布：
其中X轴是MindIE-Server组件的各个调度信息（BatchSize / Prefill序列长度），Y轴是各个调度信息的出现频次

下图为在RequestRate设置为100的情况下，Prefill阶段的BatchSize的频次分布
![Frequency_Histogram_of_Prefill_Batch_Size_100](docs/Frequency_Histogram_of_Prefill_Batch_Size_100.png)
### Benchmark多次运行的吞吐分布：
其中X轴是MindIE-Server组件的各个调度信息（BatchSize / Prefill序列长度），Y轴是吞吐相关信息 (TPS/QPS/增量时延)
下图为在RequestRate设置为100的情况下，不同的DecoderBatch取值所对应的Decode时延的分布
![ScatterPlot_of_Decoder_BatchSize_vs_Decode_Token_Latencies_100](docs/ScatterPlot_of_Decoder_BatchSize_vs_Decode_Token_Latencies_100.png)