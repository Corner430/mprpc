#include "rpcprovider.h"
#include "logger.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "zookeeperutil.h"

// 这里是框架提供给外部使用的，可以发布 rpc 方法的函数接口
// 但不进行向 zk 注册服务的操作
void RpcProvider::NotifyService(google::protobuf::Service *service) {
  ServiceInfo service_info;

  // 获取了服务对象的描述信息
  const google::protobuf::ServiceDescriptor *pserviceDesc =
      service->GetDescriptor();

  // 获取服务的名字
  std::string service_name = pserviceDesc->name();

  // 获取服务对象 service 的方法的数量
  int methodCnt = pserviceDesc->method_count();

  // std::cout << "service_name:" << service_name << std::endl;
  LOG_INFO("service_name:%s", service_name.c_str());

  for (int i = 0; i < methodCnt; ++i) {
    // 获取了服务对象指定下标的服务方法的描述（抽象描述）
    const google::protobuf::MethodDescriptor *pmethodDesc =
        pserviceDesc->method(i);
    std::string method_name = pmethodDesc->name();
    service_info.m_methodMap.insert({method_name, pmethodDesc});

    LOG_INFO("method_name:%s", method_name.c_str());
  }

  service_info.m_service = service;
  m_serviceMap.insert({service_name, service_info});
}

// 启动 rpc 服务节点，开始提供 rpc 远程网络调用服务
void RpcProvider::Run() {
  // 读取配置文件 rpcserver 的信息
  std::string ip =
      MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
  uint16_t port = atoi(MprpcApplication::GetInstance()
                           .GetConfig()
                           .Load("rpcserverport")
                           .c_str());
  muduo::net::InetAddress address(ip, port);

  // 创建 TcpServer 对象
  muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

  // 绑定连接回调和消息读写回调方法，分离了网络代码和业务代码
  server.setConnectionCallback(
      std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
  server.setMessageCallback(
      std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));

  // 设置 muduo 库的线程数量
  server.setThreadNum(4);

  // 把当前 rpc 节点上要发布的服务全部注册到 zk 上
  ZkClient zkCli;
  zkCli.Start();
  // service_name 为永久性节点，method_name为临时性节点
  for (auto &sp : m_serviceMap) {
    /* /service_name   /UserServiceRpc */
    std::string service_path = "/" + sp.first;
    zkCli.Create(service_path.c_str(), nullptr, 0);

    for (auto &mp : sp.second.m_methodMap) {
      // /service_name/method_name   /UserServiceRpc/Login
      // 存储当前这个 rpc 服务节点主机的 ip 和 port
      std::string method_path = service_path + "/" + mp.first;
      char method_path_data[128] = {0};
      sprintf(method_path_data, "%s:%d", ip.c_str(), port);
      // ZOO_EPHEMERAL 表示 znode 是一个临时性节点
      zkCli.Create(method_path.c_str(), method_path_data,
                   strlen(method_path_data), ZOO_EPHEMERAL);
    }
  }

  // rpc 服务端准备启动，打印信息
  std::cout << "RpcProvider start service at ip:" << ip << " port:" << port
            << std::endl;

  // 启动网络服务
  server.start();
  m_eventLoop.loop();
}

// 新的 socket 连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn) {
  if (!conn->connected()) // 和 rpc client 的连接断开了
    conn->shutdown();
}

/*
在框架内部，RpcProvider 和 RpcConsumer 协商好之间通信用的 protobuf 数据类型
service_name method_name args
定义 proto 的 message 类型，进行数据头的序列化和反序列化
service_name method_name args_size
16UserServiceLoginzhang san123456

header_size(4个字节) + header_str + args_str

10 "10"
10000 "1000000"
std::string   insert和copy方法
*/

// 已建立连接用户的读写事件回调
// 如果远程有一个 rpc 服务的调用请求，那么 OnMessage 方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                            muduo::net::Buffer *buffer, muduo::Timestamp) {
  // 网络上接收的远程 rpc 调用请求的字符流
  std::string recv_buf = buffer->retrieveAllAsString();

  // 从字符流中读取前 4 个字节的内容
  uint32_t header_size = 0;
  recv_buf.copy((char *)&header_size, 4, 0);

  // 根据 header_size 读取数据头的原始字符流，反序列化数据，得到 rpc
  // 请求的详细信息
  std::string rpc_header_str = recv_buf.substr(4, header_size);
  mprpc::RpcHeader rpcHeader;
  std::string service_name;
  std::string method_name;
  uint32_t args_size;
  if (rpcHeader.ParseFromString(rpc_header_str)) {
    // 数据头反序列化成功
    service_name = rpcHeader.service_name();
    method_name = rpcHeader.method_name();
    args_size = rpcHeader.args_size();
  } else { // 数据头反序列化失败
    std::cout << "rpc_header_str:" << rpc_header_str << " parse error!"
              << std::endl;
    return;
  }

  // 获取 rpc 方法参数的字符流数据
  std::string args_str = recv_buf.substr(4 + header_size, args_size);

  // 打印调试信息
  std::cout << "============================================" << std::endl;
  std::cout << "header_size: " << header_size << std::endl;
  std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
  std::cout << "service_name: " << service_name << std::endl;
  std::cout << "method_name: " << method_name << std::endl;
  std::cout << "args_str: " << args_str << std::endl;
  std::cout << "============================================" << std::endl;

  // 获取 service 对象和 method 对象
  auto it = m_serviceMap.find(service_name);
  if (it == m_serviceMap.end()) {
    std::cout << service_name << " is not exist!" << std::endl;
    return;
  }

  auto mit = it->second.m_methodMap.find(method_name);
  if (mit == it->second.m_methodMap.end()) {
    std::cout << service_name << ":" << method_name << " is not exist!"
              << std::endl;
    return;
  }

  google::protobuf::Service *service =
      it->second.m_service; // 获取 service 对象，eg: new UserService
  const google::protobuf::MethodDescriptor *method =
      mit->second; // 获取 method 对象，eg: Login

  // 生成 rpc 方法调用的请求 request 和响应 response 参数
  google::protobuf::Message *request =
      service->GetRequestPrototype(method).New();

  if (!request->ParseFromString(args_str)) {
    std::cout << "request parse error, content:" << args_str << std::endl;
    return;
  }
  google::protobuf::Message *response =
      service->GetResponsePrototype(method).New();

  // 给下面的 method 方法的调用，绑定一个 Closure 的回调函数
  google::protobuf::Closure *done =
      google::protobuf::NewCallback<RpcProvider,
                                    const muduo::net::TcpConnectionPtr &,
                                    google::protobuf::Message *>(
          this, &RpcProvider::SendRpcResponse, conn, response);

  // 在框架上根据远端 rpc 请求，调用当前 rpc 节点上发布的方法
  // new UserService().Login(controller, request, response, done)
  service->CallMethod(method, nullptr, request, response, done);
}

// Closure 的回调操作，用于序列化 rpc 的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn,
                                  google::protobuf::Message *response) {
  std::string response_str;
  if (response->SerializeToString(&response_str)) // response 进行序列化
    // 序列化成功后，通过网络把 rpc 方法执行的结果发送会 rpc 的调用方
    conn->send(response_str);
  else
    std::cout << "serialize response_str error!" << std::endl;
  conn->shutdown(); // 模拟 http 的短链接服务，由 rpcprovider 主动断开连接
}