#include "friend.pb.h"
#include "mprpcapplication.h"
#include <iostream>

int main(int argc, char **argv) {
  // 整个程序启动以后，想使用 mprpc 框架来享受rpc服务调用，
  // 一定需要先调用框架的初始化函数（只初始化一次）
  MprpcApplication::Init(argc, argv);

  /* 演示调用远程发布的 rpc 方法 Login */
  corner::FiendServiceRpc_Stub stub(new MprpcChannel());

  corner::GetFriendsListRequest request; // rpc 方法的请求参数
  request.set_userid(1000);

  corner::GetFriendsListResponse response; // rpc 方法的响应

  /* 同步发起 rpc 方法的调用：RpcChannel->RpcChannel::callMethod */
  MprpcController controller;
  stub.GetFriendsList(&controller, &request, &response, nullptr);

  // 一次 rpc 调用完成，读调用的结果
  if (controller.Failed())
    std::cout << controller.ErrorText() << std::endl;
  else {
    if (0 == response.result().errcode()) {
      std::cout << "rpc GetFriendsList response success!" << std::endl;
      int size = response.friends_size();
      for (int i = 0; i < size; ++i) {
        std::cout << "index:" << (i + 1) << " name:" << response.friends(i)
                  << std::endl;
      }
    } else {
      std::cout << "rpc GetFriendsList response error : "
                << response.result().errmsg() << std::endl;
    }
  }

  return 0;
}