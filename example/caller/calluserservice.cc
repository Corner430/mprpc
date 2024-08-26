#include "mprpcapplication.h"
#include "mprpcchannel.h"
#include "user.pb.h"
#include <iostream>

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
  stub.Login(nullptr, &request, &response, nullptr);

  // 一次 rpc 调用完成，读调用的结果
  if (0 == response.result().errcode())
    std::cout << "rpc login response success:" << response.sucess()
              << std::endl;
  else
    std::cout << "rpc login response error : " << response.result().errmsg()
              << std::endl;

  // 演示调用远程发布的 rpc 方法 Register
  corner::RegisterRequest req;
  req.set_id(2000);
  req.set_name("mprpc");
  req.set_pwd("666666");
  corner::RegisterResponse rsp;

  // 以同步的方式发起 rpc 调用请求，等待返回结果
  stub.Register(nullptr, &req, &rsp, nullptr);

  // 一次 rpc 调用完成，读调用的结果
  if (0 == rsp.result().errcode())
    std::cout << "rpc register response success:" << rsp.sucess() << std::endl;
  else
    std::cout << "rpc register response error : " << rsp.result().errmsg()
              << std::endl;

  return 0;
}