# MindIE-MS
MindIE MS（MindIE management service）作为MindIE的集群管理组件，主要服务于集群场景，根据应用场景，分为以下三个组件：部署器（Deployer）
、控制器（Controller）、调度器（Coordinator）。详细请参考[《MindIE Motor开发指南》](https://www.hiascend.com/document/detail/zh/mindie)。
## 构建方式

默认构建

```bash
bash one_key_build.sh
```

如果需要构建Debug版本，则加上 -d 参数，如下所示(默认为Release)：

```bash
bash one_key_build.sh -d
```

如果需要设置版本号，则加上 -v 版本号 参数，如下所示(默认为1.0.0)：

```bash
bash one_key_build.sh -v 1.0.0
```

## 特性支持度
此处罗列各特性适用的硬件设备。

| 特性    | Atlas 300I Duo 推理卡+Atlas 800 推理服务器（型号：3000） | Atlas 800I A2推理产品 |
|---------|---------------------------------------------|---------|
| 单机服务部署 | 支持                                          | 支持 |
| Prefix Cache | 支持                                          | 支持 |
| 分布式多机服务部署 | 不支持                                         | 支持 |
| PD分离服务部署 | 不支持                                         | 支持 |


## 环境配套
集群容器化部署依赖Kubernetes和MindX DL，详情请参见开发指南中的软件环境章节。


## MindIE MS 各个模块功能介绍
### Deployer：服务于集群部署时的简化部署场景。
一、功能描述    
部署器，底层集成Kubernetes（简称K8s）生态，主要支持对MindIE Server服务集群的一键式部署管理。
详情请参见开发指南中的MindIE MS 部署器（Deployer）章节。

二、对外接口    
MindIE MS Deployer提供命令行工具，可供用户使用，当前提供的对外命令为：

|  操作 | 命令                                           | 
|---|----------------------------------------------|
|命令行使用帮助| ./msctl -h/--help                            | 
|部署服务 | ./msctl create infer_server -f infer_server.json |
|更新服务 | ./msctl update infer_server -f {更新配置json文件名} |
|查询服务 | ./msctl get infer_server -n {服务名称}           |
|删除服务 | ./msctl delete infer_server -n {服务名称}        |

Deployer REST接口API使用详情请参见开发指南中的5.3.2.3章节。

### Controller：服务于集群运行时的运维管理场景。
一、功能描述      
控制器，完成集群内所有MindIE Server的业务状态监控、PD身份管理与决策、资源管理决策等，是整个集群的状态监控器和决策大脑。
详情请参见开发指南中的MindIE MS 控制器（Controller）章节。

二、对外接口    
MindIE MS Controller对外提供REST接口API，可供用户使用。

| 接口名称            | 接口格式                                                                           |
|-------------------|-----------------------------------------------------------------------------------|
| 启动状态查询接口     |  GET https://{ip}:{port}/v1/startup                                               |
| 健康状态查询接口     |  GET https://{ip}:{port}/v1/health                                                 |

使用示例1：启动状态查询
```
# 请求示例
GET https://{ip}:{port}/v1/startup  
# 响应示例
{
    "message":"",
    "status":"0"
}
```
使用示例2：健康状态查询
```
# 请求示例
GET https://{ip}:{port}/v1/health  
# 响应示例
{
    "message":"",
    "status":"0"
}
```
Controller REST接口API使用详情请参见开发指南中的5.3.3.4章节。

### Coordinator：集群运行时的推理请求入口场景。
一、功能描述    
调度器，是用户推理请求的入口，接收高并发的推理请求，进行请求调度、请求管理、请求转发等，是整个集群的数据请求管理入口。
详情请参见开发指南中的MindIE MS 调度器（Coordinator）章节。

二、对外接口
1. 用户侧接口

| 接口名称              | 接口格式                                                                                         |
|-------------------|----------------------------------------------------------------------------------------------|
| TGI流式推理接口         | POST https://{ip}:{port}/generate_stream                                                     |
| TGI文本推理接口         | POST https://{ip}:{port}/generate                                                            |
| TGI文本/流式推理接口      | POST https://{ip}:{port}/                                                                    |
| vLLM文本/流式推理接口     | POST https://{ip}:{port}/generate                                                            |
| OpenAI推理接口         |POST https://{ip}:{port}/v1/chat/completions                                                 |
| Triton流式推理接口      | POST https://{ip}:{port}/v2/models/{MODEL_NAME}[/versions/{MODEL_VERSION}]/generate_stream |   
| Triton Token推理接口   |POST https://{ip}:{port}/v2/models/{MODEL_NAME}[/versions/{MODEL_VERSION}]/infer           |
| Triton文本推理接口      | POST https://{ip}:{port}/v2/models/{MODEL_NAME}[/versions/{MODEL_VERSION}]/generate        |
| 自研文本/流式推理接口     | POST https://{ip}:{port}/infer                                                               |
| 计算token接口 | POST https://{ip}:{port}/v1/tokenizer                                                               |

使用示例1：TGI流式推理接口，使用单模态文本模型
```
# 请求示例
POST https://{ip}:{port}/generate_stream  
{
    "inputs": "My name is Olivier and I",
    "parameters": {
        "decoder_input_details": false,
        "details": true,
        "do_sample": true,
        "max_new_tokens": 20,
        "repetition_penalty": 1.03,
        "return_full_text": false,
        "seed": null,
        "temperature": 0.5,
        "top_k": 10,
        "top_p": 0.95,
        "truncate": null,
        "typical_p": 0.5,
        "watermark": false,
        "stop": null,
        "adapter_id": "None"
    }
}
# 响应示例
data: {"token":{"id":13,"text":"\n","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":26626,"text":"Jan","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":300,"text":"et","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":3732,"text":" makes","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":395,"text":" $","logprob":null,"special":null},"generated_text":null,"details":null}

……

data: {"token":{"id":395,"text":" $","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":29896,"text":"1","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":29896,"text":"1","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":29947,"text":"8","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":29889,"text":".","logprob":null,"special":null},"generated_text":null,"details":null}

data: {"token":{"id":2,"text":"","logprob":null,"special":null},"generated_text":"\nJanet makes $104 a day at the farmers' market.\nThe number of eggs that Janet sells at the farmers' market each day is 16 - 3 - 4 = 7.\nShe makes $2 for each egg that she sells.\nThe total amount that she makes is $2 * 7 = $14.\nThe total amount that she makes is $14 + $104 = $118.\nThe total amount that she makes is $118.","details":{"prompt_tokens":74,"finish_reason":"eos_token","generated_tokens":116,"seed":8756727004129248931}}
```

2. 集群内通信接口

| 接口名称               | 接口格式                                                             |
|--------------------|----------------------------------------------------------------------|
| 自研启动状态查询           |Get https://{ip}:{port}/v1/startup    |
| 自研健康状态查询           |Get https://{ip}:{port}/v1/health    |
| 自研就绪状态查询           |Get https://{ip}:{port}/v1/readiness   |
| triton健康状态查询       |Get https://{ip}:{port}/v2/health/live   |
| triton就绪状态查询       |Get https://{ip}:{port}/v2/health/ready   |
| triton模型就绪状态查询     |Get https://{ip}:{port}/v2/models/{model_name}/ready   |
| 集群节点同步             |POST https://{ip}:{port}/v1/instances/refresh|
| 停止节点服务             |POST https://{ip}:{port}/v1/instances/offline|
| 恢复节点服务             |POST https://{ip}:{port}/v1/instances/online|
| 查询节点请求状态           |GET https://{ip}:{port}/v1/instances/tasks?id=1&id=2&id=3|
| PD之间任务状态查询  |POST https://{ip}:{port}/v1/instances/query_tasks|
| 查询Coordinator运行状态  |GET https://{ip}:{port}/v1/coordinator_info|
| 监控指标查询  |GET https://{ip}:{port}/metrics|

Coordinator REST接口API使用详情请参见开发指南中的5.3.4.4章节。
