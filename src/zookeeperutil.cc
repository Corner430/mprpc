#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <iostream>
#include <semaphore.h>

/*
 * 一个回调函数指针，用于处理来自 ZooKeeper 服务器的事件通知。
 * 每当 ZooKeeper 中的节点发生变化或会话状态改变时，ZooKeeper 会调用这个函数。
 */
void global_watcher(zhandle_t *zh, int type, int state, const char *path,
                    void *watcherCtx) {
  if (type == ZOO_SESSION_EVENT) // 回调的消息类型是和会话相关的消息类型
    if (state == ZOO_CONNECTED_STATE) { // zkclient 和 zkserver连接成功
      // 释放一个信号量，让启动 zkclient 的线程继续往下执行
      sem_t *sem = (sem_t *)zoo_get_context(zh);
      sem_post(sem);
    }
}

ZkClient::ZkClient() : m_zhandle(nullptr) {}

ZkClient::~ZkClient() {
  if (m_zhandle != nullptr)
    zookeeper_close(m_zhandle); // 关闭句柄，释放资源
}

/* 连接 zkserver */
void ZkClient::Start() {
  std::string host =
      MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
  std::string port =
      MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
  std::string connstr = host + ":" + port;

  /*
   * zookeeper_mt：mt 是 multi-threaded 的缩写，多线程版本
   * zookeeper 的 API 客户端程序提供了三个线程
   *   1. I/O 线程：
   *      - 负责处理与 ZooKeeper 服务器的网络通信。
   *      - 在多线程环境中，I/O 线程会管理网络请求和响应，以避免单线程处理瓶颈。
   *   2. 由 watcher 触发的回调线程
   */
  m_zhandle =
      zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr,
                     0); // 创建 zk 客户端句柄，session_timeout=30000ms
  if (nullptr == m_zhandle) {
    std::cout << "zookeeper_init error!" << std::endl;
    exit(EXIT_FAILURE);
  }

  sem_t sem;
  /*
   * 第二个参数 0 表示信号量在进程间共享（此处设置为
   *                                0，表示仅在当前进程内使用）。
   * 第三个参数 0 设置初始计数为 0，表示信号量开始时是锁定状态。
   */
  sem_init(&sem, 0, 0);
  zoo_set_context(m_zhandle, &sem);

  sem_wait(&sem); // 阻塞，等待 zkclient 连接 zkserver 成功
  std::cout << "zookeeper_init success!" << std::endl;
}

void ZkClient::Create(const char *path, const char *data, int datalen,
                      int state) {
  char path_buffer[128];
  int bufferlen = sizeof(path_buffer);
  int flag;
  // 判断 path 表示的 znode 节点是否存在
  flag = zoo_exists(m_zhandle, path, 0, nullptr);
  if (ZNONODE == flag) { // 不存在
    // 创建指定 path 的 znode 节点
    flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE,
                      state, path_buffer, bufferlen);
    if (flag == ZOK)
      std::cout << "znode create success... path:" << path << std::endl;
    else {
      std::cout << "flag:" << flag << std::endl;
      std::cout << "znode create error... path:" << path << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

// 根据指定的 path，获取 znode 的值
std::string ZkClient::GetData(const char *path) {
  char buffer[64];
  int bufferlen = sizeof(buffer);
  int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
  if (flag != ZOK) {
    std::cout << "get znode error... path:" << path << std::endl;
    return "";
  } else
    return buffer;
}