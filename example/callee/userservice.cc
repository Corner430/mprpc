#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "user.pb.h"
#include <iostream>
#include <string>

/*
 * UserService 原来是一个本地服务，提供了两个进程内的本地方法
 *   1. Login
 *   2. GetFriendLists
 */
class UserService : public corner::UserServiceRpc {
public:
  bool Login(std::string name, std::string pwd) {
    std::cout << "doing local service: Login" << std::endl;
    std::cout << "name:" << name << " pwd:" << pwd << std::endl;
    return false;
  }

  bool Register(uint32_t id, std::string name, std::string pwd) {
    std::cout << "doing local service: Register" << std::endl;
    std::cout << "id:" << id << "name:" << name << " pwd:" << pwd << std::endl;
    return true;
  }

  /*
  重写基类 UserServiceRpc 的虚函数 下面这些方法都是框架直接调用的
  1. caller   ===>   Login(LoginRequest)  => muduo =>  callee
  2. callee   ===>   Login(LoginRequest)  => 交到下面重写的这个 Login 方法上
  */
  void Login(::google::protobuf::RpcController *controller,
             const ::corner::LoginRequest *request,
             ::corner::LoginResponse *response,
             ::google::protobuf::Closure *done) {
    // 框架给业务上报了请求参数 LoginRequest，应用获取相应数据做本地业务
    std::string name = request->name();
    std::string pwd = request->pwd();

    // 做本地业务
    bool login_result = Login(name, pwd);

    // 把响应写入。包括错误码、错误消息、返回值
    corner::ResultCode *code = response->mutable_result();
    code->set_errcode(0);
    code->set_errmsg("");
    response->set_sucess(login_result);

    // 执行回调操作，执行响应对象数据的序列化和网络发送（都是由框架来完成的）
    done->Run();
  }

  void Register(::google::protobuf::RpcController *controller,
                const ::corner::RegisterRequest *request,
                ::corner::RegisterResponse *response,
                ::google::protobuf::Closure *done) {
    uint32_t id = request->id();
    std::string name = request->name();
    std::string pwd = request->pwd();

    bool ret = Register(id, name, pwd);

    response->mutable_result()->set_errcode(0);
    response->mutable_result()->set_errmsg("");
    response->set_sucess(ret);

    done->Run();
  }
};

int main(int argc, char **argv) {
  MprpcApplication::Init(argc, argv); // 调用框架的初始化操作

  // provider 是一个 rpc 网络服务对象。把 UserService 对象发布到 rpc 节点上
  RpcProvider provider;
  provider.NotifyService(new UserService());

  // 启动一个 rpc 服务发布节点
  // Run 以后，进程进入阻塞状态，等待远程的 rpc 调用请求
  provider.Run();

  return 0;
}