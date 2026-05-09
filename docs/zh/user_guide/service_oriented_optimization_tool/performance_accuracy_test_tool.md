# 性能/精度测试工具

目前MindIE支持AISBench工具进行精度和性能测试，其详细使用方法请参见[AISBench工具](https://gitee.com/aisbench/benchmark)。支持的功能特性及性能测试指标详情请参见以下[表1 工具特性](#table_ptrm001)和[表2 性能测试结果指标](#table_ptrm002)。

**表 1**  工具特性<a id="table_ptrm001"></a>

|特性|AISBench|
|--|--|
|推理模式|支持Client模式下的流式推理和文本推理，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/models.md)。|
|推理引擎|支持MindIE、vLLM、SGLang、TGI和Triton推理引擎，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/models.md#%E6%9C%8D%E5%8A%A1%E5%8C%96%E6%8E%A8%E7%90%86%E5%90%8E%E7%AB%AF)。|
|数据集|支持39个开源数据集和synthetic随机数据集，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/datasets.md)。|
|发送模式|支持均匀分布和泊松分布，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/performance_benchmark.md)。|
|精度测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/accuracy_benchmark.md)。|
|性能测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/performance_benchmark.md)。|
|token推理|支持，详情请参见[链接](https://github.com/AISBench/benchmark-mindie-old?tab=readme-ov-file#%E6%94%AF%E6%8C%81%E7%9A%84%E6%80%A7%E8%83%BD%E8%AF%84%E6%B5%8B%E4%BB%BB%E5%8A%A1%E7%B1%BB%E5%9E%8B)。|
|Multi LoRA推理|支持，详情请参见[链接](https://github.com/AISBench/benchmark-mindie-old?tab=readme-ov-file#multi-lora%E5%9C%BA%E6%99%AF)。|
|Function Call测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/ais_bench/benchmark/configs/datasets/BFCL/README.md)。|
|多轮对话测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/multiturn_benchmark.md)。|
|稳态测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/stable_stage.md)。|
|压力测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/stable_stage.md#%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95%E4%BD%BF%E8%83%BD%E7%A8%B3%E6%80%81%E6%B5%8B%E8%AF%95)。|
|多任务测试|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/accuracy_benchmark.md#%E5%A4%9A%E4%BB%BB%E5%8A%A1%E6%B5%8B%E8%AF%84)。|
|过程可视化|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/performance_visualization.md)。|
|断点续测|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/accuracy_benchmark.md#%E4%B8%AD%E6%96%AD%E7%BB%AD%E6%B5%8B--%E5%A4%B1%E8%B4%A5%E7%94%A8%E4%BE%8B%E9%87%8D%E6%B5%8B)。|
|自定义数据集|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/blob/master/doc/users_guide/custom_dataset.md)。|
|支持插件化扩展|支持，详情请参见[链接](https://gitee.com/aisbench/benchmark/tree/master/plugin_examples)。|

**表 2**  性能测试结果指标<a id="table_ptrm002"></a>

|AISBench|指标含义|
|--|--|
|TTFT|Time To First Token，首token时延<br>**该指标在beam search场景下无法测量。**|
|ITL|Inter-token Latency，chunk间时延|
|TPOT|Time Per Output Token，decode token间时延，计算公式为：(E2EL - TTFT) / （OutputTokens - 1）<br>**该指标在beam search场景下无法测量**。|
|E2EL|End To End Latency，请求的端到端时延|
|InputTokens|请求的输入token数量|
|OutputTokens|请求的生成token数量|
|PrefillTokenThroughput|请求的prefill吞吐，计算公式为：InputTokens / TTFT|
|OutputTokenThroughput|请求的吞吐，计算公式为：OutputTokens / E2EL|
|Benchmark Duration|性能测试的端到端耗时|
|Total Requests|发送总请求数|
|Failed Requests|失败总请求数|
|Successful Requests|成功总请求数|
|Concurrency|平均并发连接数，计算公式为：sum(E2EL) / Benchmark Duration|
|Max Concurrency|配置并发连接数|
|Request Throughput|请求吞吐量，计算公式为：Successful Requests / Total Requests|
|Total Input Tokens|-|所有请求总的输入token数量|
|Total generated tokens|所有请求总的输出token数量|
|Input Token Throughput|本次测试的input token计算速度，计算公式为： Total Input Tokens / Benchmark Duration|
|Output Token Throughput|本次测试的output token计算速度，计算公式为：Total generated tokens / Benchmark Duration|
|Total Token Throughput|本次测试输入输出的总token计算速度，计算公式为：(Total Input Tokens + Total generated tokens)  / Benchmark Duration|
