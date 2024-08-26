/*
 * g++ main.cc test.pb.cc -o main -lprotobuf -g
 */
#include "test.pb.h"
#include <iostream>
#include <string>
using namespace corner;

int main() {
  /**********************************************************/
  // LoginResponse rsp;
  // ResultCode *rc = rsp.mutable_result(); // 获取result对象
  // rc->set_errcode(1);
  // rc->set_errmsg("登录处理失败了");

  /**********************************************************/
  // GetFriendListsResponse rsp;
  // ResultCode *rc = rsp.mutable_result();
  // rc->set_errcode(0);

  // User *user1 = rsp.add_friend_list();
  // user1->set_name("zhang san");
  // user1->set_age(20);
  // user1->set_sex(User::MAN);

  // User *user2 = rsp.add_friend_list();
  // user2->set_name("li si");
  // user2->set_age(22);
  // user2->set_sex(User::MAN);

  // std::cout << rsp.friend_list_size() << std::endl;

  /**********************************************************/

  LoginRequest req;
  req.set_name("zhang san");
  req.set_pwd("123456");

  std::string send_str;
  if (req.SerializeToString(&send_str)) // 序列化
    std::cout << send_str << std::endl;

  LoginRequest reqB;
  if (reqB.ParseFromString(send_str)) { // 反序列化
    std::cout << reqB.name() << std::endl;
    std::cout << reqB.pwd() << std::endl;
  }

  return 0;
}