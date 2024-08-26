#pragma once
#include <google/protobuf/service.h>
#include <string>

/*
 * 1. MprpcController 用于在发送 RPC 请求时携带请求的控制信息，
 *                    并在接收响应时提供状态和错误信息。
 * 2. 它可以在请求和响应之间传递必要的上下文信息。
 */
class MprpcController : public google::protobuf::RpcController {
public:
  MprpcController();
  void Reset();
  bool Failed() const;
  std::string ErrorText() const;
  void SetFailed(const std::string &reason);

  // TODO
  void StartCancel();
  bool IsCanceled() const;
  void NotifyOnCancel(google::protobuf::Closure *callback);

private:
  bool m_failed;         // RPC 方法执行过程中的状态
  std::string m_errText; // RPC 方法执行过程中的错误信息
};