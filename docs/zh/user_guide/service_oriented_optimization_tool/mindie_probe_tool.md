# MindIE探针工具

MindIE Motor提供探针脚本，使能kubernetes探针检测功能，支持启动，存活，就绪三种探针。

脚本适用于以下部署场景：

<ul>
<li>集成Controller、Coordinator和Server的部署场景。</li>
<li>仅集成Server  PD混合部署场景。</li>
</ul>

在MindIE Motor安装路径下，可找到探针入口脚本：$MIES_INSTALL_PATH/scripts/http_client_ctl/probe.sh。

<br>

**探针的使用命令如下表所示。**

**表 1**  探针的使用

|指令|类型|说明|
|--|--|--|
|`bash probe.sh startup`|启动探针|用于判断程序是否已启动，探针触发后设置了60s的超时时间。|
|`bash probe.sh liveness`|存活探针|用于发现程序进程状态是否健康，探针触发后内部设置了60s的请求超时时间。|
|`bash probe.sh readiness`|就绪探针|用于判断程序是否就绪接收流量，探针触发后设置了60s的超时时间。|

<br>

**使用的环境变量如下表所示。**

**表 2**  环境变量

|环境变量|说明|
|--|--|
|POD_IP|容器Pod IP。|
|MIES_INSTALL_PATH|MindIE Motor安装路径。|
|MINDIE_SERVER_PROBE_ONLY|仅针对Server进行状态探测的标志，设置为1时生效，适用于仅集成部署Server的PD混合场景。|
|GLOBAL_RANK_TABLE_FILE_PATH|全局ranktable文件路径，适用于集成Controller、Coordinator和Server多组件部署的场景。|
|MINDIE_UTILS_HTTP_CLIENT_CTL_CONFIG_FILE_PATH|探针配置文件的读取路径。|
|MINDIE_USE_HTTPS|是否启用HTTPS安全通信，取值为true或false，设置后优先使用该配置替代http_client_ctl.json。建议用户打开，确保通信安全。如果关闭则存在较高的网络安全风险。|
|MINDIE_CHECK_INPUTFILES_PERMISSION|用户可设置是否需要检查外部挂载文件，具体包括http_client_ctl.json以及证书相关文件。默认值为空，表示需要做权限校验。<ul><li>0：对外部挂载文件不做权限校验。</li><li>非0：对外部挂载文件做权限校验。</li></ul><br>**当用户使用MINDIE_UTILS_HTTP_CLIENT_CTL_CONFIG_FILE_PATH设置配置文件路径时，http_client_ctl.json为外部挂载文件。**|
|**注：日志相关环境变量详情请参见。**|-|

<br>

**probe.sh脚本依赖http_client_ctl组件发送HTTP请求，请求指令如下表所示。**

**表 3**  http_client_ctl命令介绍

|指令|说明|
|--|--|
|./http_client_ctl [ip] [port] [url] [timeout] [retrytime]|<ul><li>[ip]：目标IPv4或IPv6格式的IP地址。</li><li>[port]：目标端口。取值范围[1024, 65535]。</li><li>[url]：HTTP请求的URL。</li><li>[timeout]：请求超时时间，单位秒。取值范围[1, 600]。</li><li>[retrytime]：重试次数。取值范围[0, 30]。</li></ul>|
|./http_client_ctl -h/--help|命令使用帮助。|

另外，http_client_ctl还需配置http_client_ctl.json文件，其字段解释如[表4](#table13687127115213)所示。

```json
{
     "tls_enable" : true,
     "cert": {
          "ca_cert" : "./security/http_client/ca/ca.pem",
          "tls_cert": "./security/http_client/certs/cert.pem",
          "tls_key": "./security/http_client/keys/cert.key.pem",
          "tls_passwd": "./security/http_client/pass/key_pwd.txt",
          "tls_crl": ""
     },
     "log_info": {
          "log_level": "INFO",
          "to_file": false,
          "run_log_path": "/var/log/mindie-ms/run/log.txt",
          "operation_log_path": "/var/log/mindie-ms/operation/log.txt"
     }
}
```

**表 4**  http_client_ctl.json配置说明<a id="table13687127115213"></a>

|配置类型|配置项|配置介绍|
|--|--|--|
|证书配置|tls_enable|必填。是否开启HTTPS通信，默认为true。<ul><li>true：表示开启；</li><li>false：表示关闭。</li></ul>建议用户开启，确保通信安全。如果关闭则存在较高的网络安全风险。<br>如设置环境变量MINDIE_USE_HTTPS，则优先读取环境变量的值。|
|证书配置|ca_cert|必填。<br>客户端ca根证书文件路径，该路径真实存在且可读。|
|证书配置|tls_cert|必填。<br>客户端tls证书文件路径，该路径真实存在且可读。|
|证书配置|tls_key|必填。<br>客户端tls私钥文件路径，该路径真实存在且可读。|
|证书配置|tls_passwd|必填。<br>KMC加密的私钥口令的文件路径。|
|证书配置|tls_crl|必填。<br>证书吊销列表CRL文件路径，该路径存在且可读。如为空，则不进行吊销校验。|
|日志配置|log_level|可选。<br>日志级别。默认值为INFO。<ul><li>DEBUG</li><li>INFO</li><li>WARNING</li><li>ERROR</li><li>CRITICAL</li></ul>如设置环境变量MINDIEMS_LOG_LEVEL或者MINDIE_LOG_LEVEL，则优先读取环境变量的值，详情请参见日志配置。|
|日志配置|to_file|可选。<br>是否写入到文件。默认值为false。<ul><li>true：输出到文件。</li><li>false：不输出到文件。</li></ul>如设置环境变量MINDIE_LOG_TO_FILE，则优先读取环境变量的值，详情请参见日志配置。|
|日志配置|run_log_path|运行日志文件路径。建议使用环境变量配置日志，详情请参见日志配置。|
|日志配置|operation_log_path|操作日志文件路径。建议使用环境变量配置日志，详情请参见日志配置。|

若用户开启了HTTPS安全通信，需要设置上述tls_enable为true，并准备相关证书（CA证书，客户端证书，私钥文件）。

将证书导入容器，两种方式如下所示：

- 方式一：在制作镜像的过程中拷贝相关的证书到容器内。
- 方式二：在启动容器时将相关文件通过宿主机挂载方式导入。

最后需将MindIE Motor安装路径下的conf/http_client_ctl.json文件"cert"字段中的相关文件路径配置为导入容器后的绝对路径。
