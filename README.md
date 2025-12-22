# MindIE Motor


* [**MindIE官方网站**](https://www.hiascend.com/software/mindie)

* [**MindIE文档教程**](https://www.hiascend.com/document/detail/zh/mindie/10RC3/releasenote/releasenote_0001.html)

* [**支持与服务**](https://www.hiascend.com/support)


## 1. 产品简介

**MindIE Motor是面向通用模型场景的推理服务化框架，通过开放、可扩展的推理服务化平台架构提供推理服务化能力，支持对接业界主流推理框架接口，满足大语言模型的高性能推理需求**。MindIE Motor的组件包括：

* **MindIE MS**：服务策略管理，提供服务运维能力。主要功能包括模型Pod级和Pod内实例级管理、简化部署并提供服务质量监控、模型更新、故障重调度和自动扩缩负载均衡能力，不仅能够提升服务质量，同时也能提高推理硬件资源利用率。

* **MindIE Server**：推理服务端；提供模型推理服务化能力，支持命令行部署RESTful服务。

    * **EndPoint**：提供简易的API接口；EndPoint面向推理服务开发者提供极简易用的API接口，推理服务化协议和接口封装，支持Triton/OpenAI/TGI/vLLM主流推理框架请求接口。

    * **GMIS**：模型推理调度器，提供多实例调度能力；GMIS模块支持推理流程的工作流定义扩展，以工作流为驱动，实现从推理任务调度到任务执行的可扩展架构，适应各类推理方法。

    * **BackendManager**：模型执行后端，昇腾后端和自定义后端的管理模块；Backend管理模块面向不同推理引擎，不同模型，提供统一抽象接口，便于扩展，减少推理引擎、模型变化带来的修改。

* **MindIE Client**：昇腾推理服务化客户端；配套昇腾推理服务化MindIE Server提供完整的推理服务化能力，包括对接MindIE Server的通信协议、请求和返回的接口，提供给用户应用对接。

* **MindIE Service Tools**：昇腾推理服务化工具；主要功能有大模型推理性能测试、精度测试和可视化能力，并且支持通过配置提升吞吐。

* **MindIE Backends**：支持昇腾MindIE LLM后端。


## 2. 关键特性

| 特性       | 说明              |
| ------------ | ----------------- |
| **Multi-Lora**  | 使用Multi-Lora来执行基础模型和不同的LoRA权重进行推理，其特性介绍详情请参见[Multi-Lora](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0118.html)。       |
| **多模态理解**  | 多模态理解模型是指能够处理和理解包括多种模态数据的深度学习模型。其特性介绍详情请参见[多模态理解](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0121.html)。       |
| **Function Call** | 支持Function Call函数调用，使大模型具备使用工具能力。其特性介绍详情请参见[Function Call](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0124.html)。 |
| **Splitfuse** | 将长提示词分解成更小的块，并在多个forward step中进行调度，降低Prefill时延。其特性介绍详情请参见[Splitfuse](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0127.html)。 |
| **Prefix Cache** | 复用跨session的重复token序列对应的KV Cache，减少一部分前缀token的KV Cache计算时间，从而减少Prefill的时间。其特性介绍详情请参见[Prefix Cache](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0131.html)。 |
| **分布式多机部署** | 对于超大模型，单机推理无法容纳整个模型权重参数，因此需要多台推理机协同工作，共同完成整个模型的推理，其特性介绍详情请参见[分布式多机部署](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0134.html)。   |
| **PD分离部署** | 模型推理的Prefill阶段和Decode阶段分别实例化部署在不同的机器资源上同时进行推理，提升推理性能，其特性介绍详情请参见[PD分离部署](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0138.html)。 |
| **模型量化** | 其特性介绍详情请参见[模型量化](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0146.html)。 |


## 3. 快速开始

### 3.1 环境准备

请参考[MindIE安装指南](https://www.hiascend.com/document/detail/zh/mindie/10RC3/envdeployment/instg/mindie_instg_0001.html)进行环境的安装与部署。安装前，请检查硬件和操作系统是否支持。请按照对应的硬件和操作系统下载对应的安装包。支持物理机部署和容器化部署。

### 3.2 部署 MindIE Motor

不同场景的部署可能会有所差异，请根据实际需求选择合适的部署方式：

| 场景       | 部署说明              |
| ------------ | ----------------- |
| **单机推理场景**  | 详情请参见[单机推理](https://www.hiascend.com/document/detail/zh/mindie/10RC3/envdeployment/instg/mindie_instg_0026.html)。       |
| **K8s集群服务部署场景** | 支持三种部署形态：单机服务(非分布式)、分布式多机服务、PD分离服务。详情请参见[集群服务部署](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0015.html)。 |


### 3.3 配置 MindIE Motor

根据用户的部署场景，调整相应的配置参数和环境变量。由于配置项较多，具体操作请参考《MindIE Motor开发指南》的[配置参数说明](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0285.html)章节和[环境变量](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0300.html)章节。

### 3.4 启动服务

具体请参考《MindIE Motor开发指南》的[启动服务](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0004.html)章节。

### 3.5 接口调用

MindIE Server 支持部署与 Triton、OpenAI、TGI 和 vLLM 等第三方框架兼容的服务应用。通过该服务，您可以便捷地进行服务状态查询、模型信息查询以及文本或流式推理等操作。我们提供了以下接口支持：

| 接口       | 说明              |
| ------------ | ----------------- |
| **兼容TGI 0.9.4版本的接口**  | 详情请参见[使用兼容TGI 0.9.4版本的接口](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0006.html)。       |
| **兼容vLLM 0.2.6版本的接口**  | 详情请参见[使用兼容vLLM 0.2.6版本接口](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0007.html)。       |
| **兼容OpenAI接口** | 详情请参见[使用兼容OpenAI接口](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0008.html)。 |
| **兼容Triton接口** | 详情请参见[使用兼容Triton接口](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0009.html)。 |
| **MindIE原生接口** | 详情请参见[使用MindIE原生接口](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0010.html)。 |


### 3.6 性能和精度测试

精度测试样例如下所示：

```bash
benchmark \
--DatasetPath "/{数据集路径}/GSM8K" \
--DatasetType "gsm8k" \
--ModelName "baichuan2_13b" \
--ModelPath "/{模型路径}/baichuan2-13b" \
--TestType client \
--Http https://{ipAddress}:{port} \
--ManagementHttp https://{managementIpAddress}:{managementPort} \
--MaxOutputLen 512 \
--TestAccuracy True
```

性能测试样例如下所示：

```bash
benchmark \
--DatasetPath "/{数据集路径}/GSM8K" \
--DatasetType "gsm8k" \
--ModelName "baichuan2_13b" \
--ModelPath "/{模型路径}/baichuan2-13b" \
--TestType client \
--Http https://{ipAddress}:{port} \
--ManagementHttp https://{managementIpAddress}:{managementPort} \
--MaxOutputLen 512 \
```

参数详细解释请参见[输入参数](https://www.hiascend.com/document/detail/zh/mindie/10RC3/mindieservice/servicedev/mindie_service0153.html)。

## 4. 编译 MindIE

MindIE 仓库包括 MindIE-LLM 和 MindIE-Motor 等。在编译中，将MindIE-Motor视作主仓，主仓能编译 MindIE-Motor 和其他仓库，并创建 MindIE 整包。


### 4.1 一键编译

编译中会使用 `wget` 命令行工具下载软件包，如果需要使用 `--no-check-certificate` 选项忽略证书，直接下载，可运行

```bash
# wget 下载添加 --no-check-certificate 选项
export NO_CHECK_CERTIFICATE=1
```

一键编译，创建可安装的 MindIE run 包。下载仓库后，运行：

```bash
cd MindIE-Motor        # 进入仓库根目录
bash build/build.sh -a   # 编译 MindIE
```

**说明：**

- 可能需要输入gitee账号以下载 [MindIE-LLM](https://gitee.com/ascend/MindIE-LLM) 和 确保有仓库访问权限。
- 使用 `--no-check-certificate` 选项下载的软件包需要用户自行进行完整性校验

编译结束后，MindIE run包存放于 `MindIE-Motor/output/<arch>/Ascend-mindie_<version>_linux-<arch>.run`。run 安装命令为

```bash
bash Ascend-mindie_<version>_linux-<arch>.run --install --install-path=<your_dir>
```

### 4.2 MindIE 编译选项

MindIE-Motor提供针对全部仓库的构建脚本 `MindIE-Motor/build/build.sh`（下文假设已在 `MindIE-Motor` 目录），编译选项包括：

#### 自动编译：`-a | --auto`

此选项自动下载第三方依赖和 [MindIE-LLM](https://gitee.com/ascend/MindIE-LLM) 两个仓库。之后编译 MindIE-LLM 和 MindIE-Motor 2个仓库，编译完成后，创建 MindIE run包。当使用 `-a` 选项时，不要使用 `-d`、`-b`和`-p` 选项，否则可能造成冲突。

####  下载选项：`-d | --download`

使用示例

```bash
bash build/build.sh -d [modules]
```

**说明：**

- [modules] 为 {“llm”, “3rd”} 中的一个或几个，使用时不需要带中括号 `[]`。MindIE 仓库下载至 `MindIE-Motor/modules` 目录，第三方依赖下载至 `MindIE-Motor/third_party` 目录。其中 `3rd` 代表第三方依赖，`llm` 代表 MindIE-LLM。

#### 编译选项：`-b | --build`

下载选项，使用示例

```bash
bash build/build.sh -b [modules]
```

**说明：**

- [modules] 取值为 {“service”, “llm”, “3rd", “ms”, “server”, “benchmark”, “client”, “simulator”} 中的一个或几个。
- 对于 “llm”，如果代码未下载至 `MindIE-Motor/modules/` 文件夹，其会自动下载。如果存在，则将不再下载，也不对对代码做任何修改。如果需要在编译前切换分支（默认为 master 分支）或修改代码，可以进入目录后手动完成操作。
- 编译时会按 `-b` 跟上多个参数时，会根据先后顺序编译，因此应考虑模块之间的依赖关系。例如同时编译 MindIE-LLM 和 MindIE-Motor 2 个仓库，根据依赖关系，应运行 `bash build/build.sh -b llm service`。
- “backend” 属于 MindIE-LLM，已取消该子组件的单独编译。

#### 打包选项：`-p | --package`

```bash
bash build/build.sh -p  # 或 bash build/build.sh --package
```

创建 mindie 安装包，由于其他各个仓库编译后均打包，因此未单独设置每个仓库的打包脚本。

#### 帮助选项：`-h | --help`

打印参数，显示帮助信息。

```bash
bash build/build.sh -h # 或 bash build/build.sh --help
```

#### 编译类型选项： `--debug、--release、--parallel`

`--debug`: 创建 Debug 模式的二进制文件，可用于 gdb 调试。如果不指定 --debug 或 --release，默认为 release 模式。

`--release`: 创建 Release 模式的二进制文件。如果不指定 --debug 或 --release，默认为 release 模式。

`--parallel [n]`：CMake编译的线程数。较少时编译较慢；过大可能占用过量内存而kill编译进程。可根据需要配置。

#### 支持参数联合使用

例如希望一键完成整个MindIE包，则可以使用下面代码：

```bash
bash build/build.sh -d 3rd llm -b llm service -p
```

其功能等价于 `bash build/build.sh -a`。

## 5. 声明

* 本代码仓提到的数据集和模型仅作为示例，这些数据集和模型仅供您用于非商业目的，如您使用这些数据集和模型来完成示例，请您特别注意应遵守对应数据集和模型的License，如您因使用数据集或模型而产生侵权纠纷，华为不承担任何责任。
* 如您在使用本代码仓的过程中，发现任何问题（包括但不限于功能问题、合规问题），请在本代码仓提交issue，我们将及时审视并解答。
