#include "mprpcapplication.h"
#include <iostream>
#include <string>
#include <unistd.h>

MprpcConfig MprpcApplication::m_config;

void ShowArgsHelp() {
  std::cout << "format: command -i <configfile>" << std::endl;
}

void MprpcApplication::Init(int argc, char **argv) {
  if (argc < 2) {
    ShowArgsHelp();
    exit(EXIT_FAILURE);
  }

  int c = 0;
  std::string config_file; // 用于保存配置文件的路径
  while ((c = getopt(argc, argv, "i:")) != -1) {
    switch (c) {
    case 'i':
      config_file = optarg; // 获取 -i 选项后面的参数（配置文件的路径）
      break;
    case '?': // 无效选项
      ShowArgsHelp();
      exit(EXIT_FAILURE);
    case ':': // 缺少选项参数
      ShowArgsHelp();
      exit(EXIT_FAILURE);
    default:
      break;
    }
  }

  // 获取 rpcserver_ip, rpcserver_port, zookeeper_ip, zookeeper_port
  m_config.LoadConfigFile(config_file.c_str());

  /* 调试代码，测试配置文件是否读取成功 */
  // std::cout << "rpcserverip:" << m_config.Load("rpcserverip") << std::endl;
  // std::cout << "rpcserverport:" << m_config.Load("rpcserverport") <<
  // std::endl; std::cout << "zookeeperip:" << m_config.Load("zookeeperip") <<
  // std::endl; std::cout << "zookeeperport:" << m_config.Load("zookeeperport")
  // << std::endl;
}

MprpcApplication &MprpcApplication::GetInstance() {
  static MprpcApplication app;
  return app;
}

MprpcConfig &MprpcApplication::GetConfig() { return m_config; }