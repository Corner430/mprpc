#pragma once
#include "google/protobuf/service.h"
#include <functional>
#include <google/protobuf/descriptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <string>
#include <unordered_map>

// 框架提供的专门发布 rpc 服务的网络对象类
class RpcProvider {
public:
  // 这里是框架提供给外部使用的，可以发布 rpc 方法的函数接口
  // 但不进行向 zk 注册服务的操作
  void NotifyService(google::protobuf::Service *service);

  /*
   * 启动 rpc 服务节点，开始提供 rpc 远程网络调用服务
   *   1. 读取配置文件 rpcserver 的信息
   *   2. 创建 TcpServer 对象
   *   3. 绑定连接回调和消息读写回调方法，分离了网络代码和业务代码
   *   4. 设置 muduo 库的线程数量
   *   5. 把当前 rpc 节点上要发布的服务全部注册到 zk 上
   *   6. 启动 TcpServer 对象
   *   7. 进入事件循环
   */
  void Run();

private:
  muduo::net::EventLoop m_eventLoop; // 组合EventLoop

  struct ServiceInfo {                    // service服务类型信息
    google::protobuf::Service *m_service; // 保存服务对象
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor *>
        m_methodMap; // 保存服务方法
  };

  // 存储注册成功的服务对象和其服务方法的所有信息
  std::unordered_map<std::string, ServiceInfo> m_serviceMap;

  // 新的 socket 连接回调
  void OnConnection(const muduo::net::TcpConnectionPtr &);

  // 已建立连接用户的读写事件回调
  void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *,
                 muduo::Timestamp);

  // Closure 的回调操作，用于序列化 rpc 的响应和网络发送
  // 通过这个回调操作，把 rpc 方法的响应写回到网络
  void SendRpcResponse(const muduo::net::TcpConnectionPtr &,
                       google::protobuf::Message *);
};