#pragma once

#include <semaphore.h>
#include <string>
#include <zookeeper/zookeeper.h>

// 封装的zk客户端类
class ZkClient {
public:
  ZkClient();
  ~ZkClient();
  void Start(); // zkclient启动连接zkserver

  // 在zkserver上根据指定的path创建znode节点
  void Create(const char *path, const char *data, int datalen, int state = 0);
  // 根据参数指定的znode节点路径，或者znode节点的值
  std::string GetData(const char *path);

private:
  zhandle_t *m_zhandle; // zk的客户端句柄
};