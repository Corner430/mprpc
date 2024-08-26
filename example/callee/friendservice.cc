#include "friend.pb.h"
#include "logger.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include <iostream>
#include <string>
#include <vector>

class FriendService : public corner::FiendServiceRpc {
public:
  std::vector<std::string> GetFriendsList(uint32_t userid) {
    std::cout << "do GetFriendsList service! userid:" << userid << std::endl;
    std::vector<std::string> vec;
    vec.push_back("gao yang");
    vec.push_back("liu hong");
    vec.push_back("wang shuo");
    return vec;
  }

  // 重写基类方法
  void GetFriendsList(::google::protobuf::RpcController *controller,
                      const ::corner::GetFriendsListRequest *request,
                      ::corner::GetFriendsListResponse *response,
                      ::google::protobuf::Closure *done) {
    uint32_t userid = request->userid();
    std::vector<std::string> friendsList = GetFriendsList(userid);
    response->mutable_result()->set_errcode(0);
    response->mutable_result()->set_errmsg("");
    for (std::string &name : friendsList) {
      std::string *p = response->add_friends();
      *p = name;
    }
    done->Run();
  }
};

int main(int argc, char **argv) {
  LOG_ERR("ddddd");
  LOG_INFO("ddddd");

  // 调用框架的初始化操作
  MprpcApplication::Init(argc, argv);

  // provider 是一个 rpc 网络服务对象。把 FriendService 对象发布到 rpc 节点上
  RpcProvider provider;
  provider.NotifyService(new FriendService());

  // 启动一个 rpc 服务发布节点
  // Run 以后，进程进入阻塞状态，等待远程的 rpc 调用请求
  provider.Run();

  return 0;
}