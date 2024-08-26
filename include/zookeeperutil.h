#pragma once

#include <semaphore.h>
#include <string>
#include <zookeeper/zookeeper.h>

// 封装的 zk 客户端类
class ZkClient {
public:
  ZkClient();
  ~ZkClient();
  void Start(); // zkclient 启动连接 zkserver

  // 在 zkserver 上根据指定的 path 创建 znode 节点
  void Create(const char *path, const char *data, int datalen, int state = 0);

  // 根据参数指定的 znode 节点路径获取 znode 节点的值
  std::string GetData(const char *path);

private:
  zhandle_t *m_zhandle; // zk 的客户端句柄
};