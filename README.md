- [分布式网络通信 rpc 框架](#分布式网络通信-rpc-框架)
  - [1 RPC 概述](#1-rpc-概述)
    - [1.1 RPC 调用过程](#11-rpc-调用过程)
    - [1.2 序列化和反序列化](#12-序列化和反序列化)
    - [1.3 数据传输格式](#13-数据传输格式)
  - [2 框架](#2-框架)
    - [2.1 业务层实现](#21-业务层实现)
      - [2.1.1 数据结构定义](#211-数据结构定义)
      - [2.1.2 caller](#212-caller)
      - [2.1.3 callee](#213-callee)
    - [2.2 zookeeper](#22-zookeeper)
    - [2.3 日志](#23-日志)
  - [3 总结](#3-总结)
  - [4 项目结构](#4-项目结构)

1. 使用方式：包含 `include` 目录，链接 `lib` 目录下的 `libmprpc.a` 库文件，即可使用 mprpc 框架。具体使用方式可参考 `example` 目录下的示例代码。
2. 开发环境：muduo、protobuf、zookeeper、C++11
   - muduo 安装参考 [chatserver 单机版](https://github.com/Corner430/Docker)
   - protobuf 和 zookeeper 安装：`sudo apt install protobuf-compiler zookeeper zookeeper-bin libzookeeper-mt-dev`
   - 还要单独[安装一下 zookeeper 的服务端和客户端](https://zookeeper.apache.org/releases.html)

# 分布式网络通信 rpc 框架

## 1 RPC 概述

![20240826150050](https://cdn.jsdelivr.net/gh/Corner430/Picture/images/20240826150050.png)

### 1.1 RPC 调用过程

完整的 RPC 过程如下图：

![20240826150135](https://cdn.jsdelivr.net/gh/Corner430/Picture/images/20240826150135.png)

远程调用需传递**服务对象、函数方法、函数参数**，经序列化成字节流后传给提供服务的服务器，服务器接收到数据后反序列化成**服务对象、函数方法、函数参数**，并发起本地调用，将响应结果序列化成字节流，发送给调用方，调用方接收到后反序列化得到结果，并传给本地调用。

### 1.2 序列化和反序列化

- 序列化：对象转为字节序列称为对象的序列化
- 反序列化：字节序列转为对象称为对象的反序列化

![20240826150218](https://cdn.jsdelivr.net/gh/Corner430/Picture/images/20240826150218.png)

常见序列化和反序列化协议有 XML、JSON、PB，相比于其他 PB 更有优势：跨平台语言支持，序列化和反序列化效率高速度快，且序列化后体积比 XML 和 JSON 都小很多，适合网络传输。

|          |        XML         |        JSON        |              PB              |
| :------: | :----------------: | :----------------: | :--------------------------: |
| 保存方式 |        文本        |        文本        |            二进制            |
|  可读性  |        较好        |        较好        |            不可读            |
| 解析效率 |         慢         |        一般        |              快              |
| 语言支持 |      所有语言      |      所有语言      | C++/Java/Python 及第三方支持 |
| 适用范围 | 文件存储、数据交互 | 文件存储、数据交互 |      文件存储、数据交互      |

注意：序列化和反序列化可能对系统的消耗较大，因此原则是：**远程调用函数传入参数和返回值对象要尽量简单**，具体来说应避免：

- **远程调用函数传入参数和返回值对象体积较大**，如传入参数是 List 或 Map，序列化后字节长度较长，对网络负担较大
- **远程调用函数传入参数和返回值对象有复杂关系**，传入参数和返回值对象有复杂的嵌套、包含、聚合关系等，性能开销大
- **远程调用函数传入参数和返回值对象继承关系复杂**，性能开销大

### 1.3 数据传输格式

考虑到可靠性和长连接，因此使用 TCP 协议，而 TCP 是字节流协议，因此需自己处理拆包粘包问题，即自定义数据传输格式！如图：

![20240826150310](https://cdn.jsdelivr.net/gh/Corner430/Picture/images/20240826150310.png)

定义 protobuf 类型的结构体消息 RpcHeader，包含**服务对象、函数方法、函数参数**，因为参数可变，参数长不定，因此不能和 RpcHeader 一起定义，否则多少函数就有多少 RpcHeader，因此需为每个函数定义不同 protobuf 结构体消息，然后对该结构体消息序列化（字符串形式存储），就得到两个序列化后的二进制字符串，拼接起来就是要发送的消息，同时消息前需记录序列化后的 RpcHeader 数据的长度，这样才能分开 RpcHeader 和函数参数的二进制数据，从而反序列化得到函数参数

```protobuf
message RpcHeader { //消息头
  bytes service_name = 1;
  bytes method_name = 2;
  uint32 args_size = 3;
}
```

## 2 框架

RPC 通信过程中的代码调用流程图如图：

![20240826150403](https://cdn.jsdelivr.net/gh/Corner430/Picture/images/20240826150403.png)

### 2.1 业务层实现

#### 2.1.1 数据结构定义

以 `user.proto` 中的`Login`函数请求参数为例说明参数结构体定义，其他函数类似：

```protobuf
message LoginRequest { // Login 函数的参数
  bytes name = 1;
  bytes pwd = 2;
}
```

使用 protoc 编译 `proto` 文件：

```sh
protoc user.proto -I ./ -cpp_out=./user
```

得到 `user.pb.cc` 和 `user.pb.h`，**每个 message 结构体都生成一个类**，如上面说的 `LoginRequest` 生成继承自 `google::protobuf::Message` 的 `LoginRequest` 类，主要包含定义的私有成员变量及读取设置变量的成员函数，如 `name` 对应的`name()`和`set_name()`两个读取和设置函数；**每个 `service` 结构体都生成两个关键类**，以 `user.proto` 中定义的 `UserServiceRpc` 为例，生成继承自 `google::protobuf::Service` 的**`UserServiceRpc`**和继承自 `UserServiceRpc` 的** `UserServiceRpc_Stub` **类，前者给服务方 `callee` 使用，后者给调用方 `caller` 使用，并且都生成 `Login` 和 `Register` 虚函数，`UserServiceRpc` 类中还包括重要的 `CallMethod` 方法

```protobuf
service UserServiceRpc {
  rpc Login(LoginRequest) returns(LoginResponse);
  rpc Register(RegisterRequest) returns(RegisterResponse);
}
```

以 `Login` 方法为例，Caller 调用远程方法 `Login`，Callee 中的 `Login` 接收 `LoginRequest` 消息体，执行完 `Login` 后将结果写入 `LoginResponse` 消息体，再返回给 `Caller`

#### 2.1.2 caller

调用方将继承自 `google::protobuf::RpcChannel` 的 `MprpcChannel` 的对象，传入 `UserServiceRpc_Stub` 构造函数生成对象 `stub`，设置远程调用`Login`的请求参数 `request`，并由 `stub` 调用成员函数 `Login` 一直阻塞等待远程调用的响应，而`Login`函数实际被传入的 `channel` 对象调用`CallMethod`，在`CallMethod`中设置`controller`对象和`response`对象，前者在函数出错时设置 `rpc` 调用过程的状态，后者是远程调用的响应

```cpp
int main(int argc, char **argv) {
  // 整个程序启动以后，想使用 mprpc 框架来享受 rpc 服务调用
  // 一定需要先调用框架的初始化函数（只初始化一次）
  MprpcApplication::Init(argc, argv);

  // 演示调用远程发布的 rpc 方法 Login
  corner::UserServiceRpc_Stub stub(new MprpcChannel());

  corner::LoginRequest request; // rpc 方法的请求参数
  request.set_name("zhang san");
  request.set_pwd("123456");

  corner::LoginResponse response; // rpc 方法的响应

  /* 同步发起 rpc 方法的调用：RpcChannel->RpcChannel::callMethod */
  MprpcController controller;
  stub.Login(nullptr, &request, &response, nullptr);
  if (controller.Failed())
    std::cout << controller.ErrorText() << std::endl;
  else {
    if (0 == response.result().errcode())
        std::cout << "rpc login response success:" << response.sucess()
                << std::endl;
    else
        std::cout << "rpc login response error : " << response.result().errmsg()
                << std::endl;
  }
}
```

所有通过 `stub` 代理对象调用的 rpc 方法，通过 **C++ 多态**最终都会通过调用 `CallMethod` 实现，该函数首先序列化并拼接发送的 `send_rpc_str` 字符串，其次从 zk 服务器中拿到注册的 rpc 服务端的 ip 和 port，连接到 rpc 服务器并发送请求，接受服务端返回的字节流，并反序列化响应 `response`

```cpp
// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                google::protobuf::RpcController* controller,
                                const google::protobuf::Message* request,
                                google::protobuf::Message* response,
                                google::protobuf:: Closure* done)
{
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name(); // service_name
    std::string method_name = method->name(); // method_name

    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        return;
    }

    // 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4)); // header_size
    send_rpc_str += rpc_header_str; // rpcheader
    send_rpc_str += args_str; // args

    // 使用tcp编程，完成rpc方法的远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
    ZkClient zkCli;
    zkCli.Start();
    //  /UserServiceRpc/Login
    std::string method_path = "/" + service_name + "/" + method_name;
    // 127.0.0.1:8000
    std::string host_data = zkCli.GetData(method_path.c_str());
    if (host_data == "")
    {
        controller->SetFailed(method_path + " is not exist!");
        return;
    }
    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx+1, host_data.size()-idx).c_str());

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 发送rpc请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 反序列化rpc调用的响应数据
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }

    close(clientfd);
}
```

#### 2.1.3 callee

服务方将继承自 UserServiceRpc 的 UserService 类的对象，传入 RpcProvider 类的构造函数生成对象 provider，provider 是 rpc 服务对象，调用`NotifyService`将 UserService 对象发布到 rpc 节点上，调用`Run`启动 rpc 服务节点，提供 rpc 远程调用服务

```cpp
int main(int argc, char **argv)
{
    // 调用框架的初始化操作
    MprpcApplication::Init(argc, argv);

    // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new UserService());
    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}
```

`NotifyService`将传入进来的服务对象 service 发布到 rpc 节点上。其实就是将服务对象及其方法的抽象描述，存储在 map 中

```cpp
// 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();

    LOG_INFO("service_name:%s", service_name.c_str());

    for (int i=0; i < methodCnt; ++i)
    {
        // 获取了服务对象指定下标的服务方法的描述（抽象描述） UserService   Login
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});

        LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}
```

`Run`创建 TcpServer 对象并绑定连接回调和消息可读回调及线程数，将 rpc 节点上要发布的服务全注册到 zk 服务器上，并启动网络服务和事件循环，等待客户端的连接和写入，从而触发对应的回调

```cpp
// 启动rpc服务节点，开始提供rpc远程网络调用服务
void RpcProvider::Run()
{
    // 读取配置文件rpcserver的信息
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    // 绑定连接回调和消息读写回调方法  分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4);

    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以从zk上发现服务
    ZkClient zkCli;
    zkCli.Start();
    // service_name为永久性节点    method_name为临时性节点
    for (auto &sp : m_serviceMap)
    {
        // /service_name   /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for (auto &mp : sp.second.m_methodMap)
        {
            // /service_name/method_name   /UserServiceRpc/Login 存储当前这个rpc服务节点主机的ip和port
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL表示znode是一个临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    // rpc服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    // 启动网络服务
    server.start();
    m_eventLoop.loop();
}
```

连接回调教简单，不在赘述，主要解释可读事件的回调`OnMessage`，接收远程 rpc 调用请求的字节流并反序列化 ，解析出 service_name 和 method_name 和 args_str 参数，并查找存储服务对象的 map，找到服务对象 service 和方法对象描述符 method，生成 rpc 方法调用的请求 request 和响应 response，并设置发送响应的回调`SendRpcResponse`，并调用`CallMethod`

```cpp
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                            muduo::net::Buffer *buffer,
                            muduo::Timestamp)
{
    // 网络上接收的远程rpc调用请求的字符流    Login args
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字符流中读取前4个字节的内容
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);

    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        // 数据头反序列化失败
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return;
    }

    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end())
    {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }

    google::protobuf::Service *service = it->second.m_service; // 获取service对象  new UserService
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象  Login

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<RpcProvider,
                                                                    const muduo::net::TcpConnectionPtr&,
                                                                    google::protobuf::Message*>
                                                                    (this,
                                                                    &RpcProvider::SendRpcResponse,
                                                                    conn, response);

    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    // new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request, response, done);
}
```

通过`CallMethod`调用 UserService 中的`Login`函数，获取参数并调用本地的`Login`函数，并写入响应，调用前面设置的 response 回调函数`SendRpcResponse`

```cpp
void Login(::google::protobuf::RpcController* controller,
                       const ::corner::LoginRequest* request,
                       ::corner::LoginResponse* response,
                       ::google::protobuf::Closure* done)
    {
        // 框架给业务上报了请求参数LoginRequest，应用获取相应数据做本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 做本地业务
        bool login_result = Login(name, pwd);

        // 把响应写入  包括错误码、错误消息、返回值
        corner::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(login_result);

        // 执行回调操作   执行响应对象数据的序列化和网络发送（都是由框架来完成的）
        done->Run();
    }
```

`SendRpcResponse`函数用于将响应序列化并发送出去

```cpp
// Closure的回调操作，用于序列化rpc的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) // response进行序列化
    {
        // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }
    conn->shutdown(); // 模拟http的短链接服务，由rpcprovider主动断开连接
}
```

### 2.2 zookeeper

[Zookeeper 原理详解](https://wxler.github.io/2021/03/01/175946/#42-zxid%E5%92%8Cmyid)

- [Zookeeper C API 库](https://packages.debian.org/source/sid/zookeeper)分为单线程（zookeeper_st）和多线程库（zookeeper_mt）两种：
  - 单线程库（zookeeper_st）：仅提供异步 API 和回调函数；
  - 多线程库（zookeeper_mt）：支持同步 API 和异步 API 以及回调，包含一个 IO 线程和一个事件调度线程，用于处理连接和回调。

> 注意 `mt` 版本需要一个 `THREADED` 宏定义来触发

实现的 rpc，发起的 rpc 请求需知道请求的服务在哪台机器，所以需要分布式服务配置中心，所有提供 rpc 的节点，都需向配置中心注册服务，ip+port+服务，当然 zookeeper 不止分布式服务配置，还有其他协调功能，如分布式锁，这里不细述。

callee 启动时，将 UserService 对象发布到 rpc 节点上，也就是将每个类的方法所对应的**分布式节点地址和端口**记录在 zk 服务器上，当调用远程 rpc 方法时，就去 zk 服务器上面查询对应要调用的服务的 ip 和端口

注册服务：

```cpp
// 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以从zk上发现服务
ZkClient zkCli;
zkCli.Start();  // 内部会通过信号量进行同步阻塞，等待连接成功
// service_name为永久性节点    method_name为临时性节点
for (auto &sp : m_serviceMap)
{
    // /service_name   /UserServiceRpc
    std::string service_path = "/" + sp.first;
    zkCli.Create(service_path.c_str(), nullptr, 0);
    for (auto &mp : sp.second.m_methodMap)
    {
        // /service_name/method_name   /UserServiceRpc/Login 存储当前这个rpc服务节点主机的ip和port
        std::string method_path = service_path + "/" + mp.first;
        char method_path_data[128] = {0};
        sprintf(method_path_data, "%s:%d", ip.c_str(), port);
        // ZOO_EPHEMERAL表示znode是一个临时性节点
        zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
    }
}
```

查询服务：

```cpp
// rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
ZkClient zkCli;
zkCli.Start();
//  /UserServiceRpc/Login
std::string method_path = "/" + service_name + "/" + method_name;
// 127.0.0.1:8000
std::string host_data = zkCli.GetData(method_path.c_str());
```

### 2.3 日志

借助线程安全的消息队列（日志队列为空则线程进入 wait 状态，否则唤醒线程），将要写的日志都压入消息队列中，再单独开一个线程负责将消息队列的日志写入到磁盘文件中

```cpp
Logger::Logger()
{
    // 启动专门的写日志线程
    std::thread writeLogTask([&](){
        for (;;)
        {
            // 获取当前的日期，然后取日志信息，写入相应的日志文件当中 a+
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year+1900, nowtm->tm_mon+1, nowtm->tm_mday);

            FILE *pf = fopen(file_name, "a+");
            if (pf == nullptr)
            {
                std::cout << "logger file : " << file_name << " open error!" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::string msg = m_lckQue.Pop();

            char time_buf[128] = {0};
            sprintf(time_buf, "%d:%d:%d =>[%s] ",
                    nowtm->tm_hour,
                    nowtm->tm_min,
                    nowtm->tm_sec,
                    (m_loglevel == INFO ? "info" : "error"));
            msg.insert(0, time_buf);
            msg.append("\n");

            fputs(msg.c_str(), pf);
            fclose(pf);
        }
    });
    // 设置分离线程，守护线程
    writeLogTask.detach();
}
```

## 3 总结

整个系统时序图如下所示

![20240826150557](https://cdn.jsdelivr.net/gh/Corner430/Picture/images/20240826150557.png)

## 4 项目结构

```shell
.
├── autobuild.sh                    # 自动构建脚本，用于配置和执行构建过程
├── bin                             # 可执行文件目录
│   ├── consumer                    # 消费者可执行文件，即调用方
│   ├── provider                    # 提供者可执行文件，即服务方
│   └── test.conf                   # 测试配置文件
├── build                           # 项目编译文件目录
├── CMakeLists.txt                  # 顶层 CMake 构建配置文件
├── example                         # 框架使用示例代码
│   ├── callee                      # 示例中服务的实现
│   │   ├── CMakeLists.txt          # callee 子目录的 CMake 构建配置文件
│   │   ├── friendservice.cc
│   │   └── userservice.cc
│   ├── caller                      # 示例中服务调用的客户端实现
│   │   ├── callfriendservice.cc
│   │   ├── calluserservice.cc
│   │   └── CMakeLists.txt          # caller 子目录的 CMake 构建配置文件
│   ├── CMakeLists.txt              # example 目录的 CMake 构建配置文件
│   ├── friend.pb.cc
│   ├── friend.pb.h
│   ├── friend.proto                # Friend 相关服务（获取好友列表）的 protobuf 定义文件
│   ├── user.pb.cc
│   ├── user.pb.h
│   └── user.proto                  # User 相关服务（登记注册）的 protobuf 定义文件
├── lib                             # 库文件目录
│   └── libmprpc.a                  # 编译生成的 mprpc 静态库
├── include                         # 头文件目录
│   ├── lockqueue.h                 # 锁队列的头文件
│   ├── logger.h                    # 日志记录器的头文件
│   ├── mprpcapplication.h          # mprpc 框架的单例基础类，负责框架的一些初始化操作
│   ├── mprpcchannel.h              # 在此处统一做 rpc 方法调用的数据数据序列化和网络发送，实现 CallMethod
│   ├── mprpcconfig.h               # 读取配置文件：rpcserverip:port, zookeeperip:port
│   ├── mprpccontroller.h           # 可以在请求和响应之间传递必要的上下文信息，提供给 CallMethod 使用
│   ├── rpcheader.pb.h              # 由 protobuf 生成的 RPC 头文件
│   ├── rpcprovider.h               #
│   └── zookeeperutil.h             # 封装的 ZooKeeper 客户端类
├── README.md                       # 项目说明文件
├── src                             # 源代码目录
│   ├── CMakeLists.txt              # src 目录的 CMake 构建配置文件
│   ├── logger.cc
│   ├── mprpcapplication.cc
│   ├── mprpcchannel.cc
│   ├── mprpcconfig.cc
│   ├── mprpccontroller.cc
│   ├── rpcheader.pb.cc
│   ├── rpcheader.proto            # RPC 协议的 protobuf 定义文件
│   ├── rpcprovider.cc
│   └── zookeeperutil.cc
└── test                           # 测试代码目录
    └── protobuf                   # 处理 protobuf 的测试
        ├── main                   # 编译生成的测试可执行文件
        ├── main.cc                # 测试的主程序文件
        ├── test.pb.cc
        ├── test.pb.h
        └── test.proto             # 测试使用的 protobuf 定义文件
```
