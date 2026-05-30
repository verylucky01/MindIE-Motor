# 版本配套说明

## 产品版本信息

| 项目 | 内容 |
| ---- | ---- |
| 产品名称 | MindIE Motor |
| 产品版本 | 3.0.0 |
| 版本类型 | 正式版本 |
| 维护周期 | 三个月 |

## 相关产品版本配套说明

| 产品名称 | 版本 |
| -------- | ---- |
| CANN | 8.5.1 |
| Ascend Extension for PyTorch | 7.3.0 |
| Ascend HDK | 版本配套关系参见 [CANN版本配套说明](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850/releasenote/releasenote_0000.html) |

## 病毒扫描结果

详细信息请参见《MindIE Motor 3.0.0 virus scan report.docx》。

# 版本兼容性说明

MindIE各组件需要配套使用，请勿跨版本混用各组件。

**表 1**  软件版本兼容性说明

| MindIE | CANN | MindCluster | Ascend Extension for PyTorch | CCAE |
| ------ | ---- | ----------- | ----------------------------- | ---- |
| 3.0.0 | 8.5.1 | 7.3.0 | 7.3.0 | iMaster CCAE V100R026C00SPC010 |

# 版本使用注意事项

无

# 3.0.0更新说明

## 新增特性

* MindIE Motor支持部署在智算节点上；
* 大规模专家并行多MindIE Motor多实例场景主备支持同一个ETCD三副本；
* MindIE Motor支持PD实例业务面接口健康监测和告警；
* MindIE Motor Coordinator组件，新增CPU使用率查询功能，基于使用率与目标门限对比，超过门限，则停止接收请求，实现流控。

## 修改特性

本版本继承本产品2.3.0版本的所有特性。

## 删除特性

无

## 接口变更说明

无

## 已解决的问题

此版本解决2.3.0版本中遗留的问题：

* 单节点连续故障（如P恢复后再次故障）、P和D先后或同时故障、以及多实例并发故障，由于当前Controller到HCCP的流程在GRT表更新后缺少完善的退出机制，会导致系统陷入在roleswitching的循环中，必须等待超时后才能进行重调度。
* mindie metrics接口返回报文http头为application/json与Prometheus要求text/plain格式不相符。
* 日志报错hccl文件不存在，在hccl.json文件拷贝失败时即报错。
* 单节点连续故障（如P恢复后再次故障），由于当前建链link与unlink流程无法并发执行，导致对端建链失败，从而导致故障扩散。
* 资料中MindIE对于HDK版本适配说明不完整。

## 遗留问题

| 序号 | 遗留问题                                             | 规避手段 |
| ---- | ---------------------------------------------------- | -------- |
| 1    | D实例强制上下电场景下的缩P保D故障恢复时间大于5分钟。 | 不涉及   |

# 升级影响

## 升级过程对现行系统的影响

* 对业务的影响
软件版本升级过程中会导致业务中断。

* 对网络通信的影响
对网络通信无影响。

## 升级后对现行系统的影响

* 对业务的影响
本版本支持 MindIE Motor 部署在智算节点上，相应调整了 `user_config` 中 Motor 的默认部署为通算。若现网无通算资源、或仅使用智算场景却沿用升级前的默认配置，可能导致 Motor 无法正常拉起。升级后请结合实际节点类型与部署形态，参照《大规模专家并行方案特性指南》，核对并调整 `user_config` 中与 Motor 部署目标相关的项。

* 对网络通信的影响
对网络通信无影响。

# 漏洞修补列表

| 软件名称 | 软件版本 | CVE编号 | 实际CVSS得分 | 漏洞描述 | 解决版本  |
|---|---|---|---|---|---|
| Transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14921 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must visit a malicious page or open a malicious file.The specific flaw exists within the parsing of model files. The issue results from the lack of proper validation of user-supplied data, which can result in deserialization of untrusted data. An attacker can leverage this vulnerability to execute code in the context of the current user. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14924 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must visit a malicious page or open a malicious file.The specific flaw exists within the parsing of checkpoints. The issue results from the lack of proper validation of user-supplied data, which can result in deserialization of untrusted data. An attacker can leverage this vulnerability to execute code in the context of the current process. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14930 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must visit a malicious page or open a malicious file.The specific flaw exists within the parsing of weights. The issue results from the lack of proper validation of user-supplied data, which can result in deserialization of untrusted data. An attacker can leverage this vulnerability to execute code in the context of the current process. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14920 | 0 | A vulnerability, which was classified as critical, has been found in Hugging Face transformers (affected version not known).Using CWE to declare the problem leads to CWE-502. The product deserializes untrusted data without sufficiently verifying that the resulting data will be valid.Impacted is confidentiality, integrity, and availability.There is no information about possible countermeasures known. It may be suggested to replace the affected object with an alternative product. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14929 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must visit a malicious page or open a malicious file.The specific flaw exists within the parsing of checkpoints. The issue results from the lack of proper validation of user-supplied data, which can result in deserialization of untrusted data. An attacker can leverage this vulnerability to execute code in the context of the current process. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14926 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must convert a malicious checkpoint.The specific flaw exists within the convert_config function. The issue results from the lack of proper validation of a user-supplied string before using it to execute Python code. An attacker can leverage this vulnerability to execute code in the context of the current user. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14927 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must convert a malicious checkpoint.The specific flaw exists within the convert_config function. The issue results from the lack of proper validation of a user-supplied string before using it to execute Python code. An attacker can leverage this vulnerability to execute code in the context of the current user. | MindIE 3.0.0  |
| transformers | 4.30.2,4.33.0,4.33.1,4.34.1,4.35.0,4.36.0,4.36.2,4.37.0,4.37.1,4.37.2,4.38.2,4.39.0,4.40.0,4.42.0,4.42.4,4.43.1,4.43.2,4.44.0,4.46.2,4.49.0,4.51.0 | CVE-2025-14928 | 0 | This vulnerability allows remote attackers to execute arbitrary code on affected installations of Hugging Face Transformers. User interaction is required to exploit this vulnerability in that the target must convert a malicious checkpoint.The specific flaw exists within the convert_config function. The issue results from the lack of proper validation of a user-supplied string before using it to execute Python code. An attacker can leverage this vulnerability to execute code in the context of the current user. | MindIE 3.0.0  |
| jinja2 | 3.1.3,3.1.4 | CVE-2024-56201 | 5.4 | Jinja is an extensible templating engine. In versions on the 3.x branch prior to 3.1.5, a bug in the Jinja compiler allows an attacker that controls both the content and filename of a template to execute arbitrary Python code, regardless of if Jinja_x27;s sandbox is used. To exploit the vulnerability, an attacker needs to control both the filename and the contents of a template. Whether that is the case depends on the type of application using Jinja. This vulnerability impacts users of applications which execute untrusted templates where the template author can also choose the template filename. This vulnerability is fixed in 3.1.5. | MindIE 3.0.0  |

注：实际CVSS得分为0，即产品无实际漏洞攻击场景，不受漏洞影响（代码未编译、代码无调用、编译选项保护等）。
