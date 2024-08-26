#include "mprpcconfig.h"

#include <iostream>
#include <string>

/* 负责解析加载配置文件 */
void MprpcConfig::LoadConfigFile(const char *config_file) {
  FILE *pf = fopen(config_file, "r");
  if (nullptr == pf) {
    std::cout << config_file << " is note exist!" << std::endl;
    exit(EXIT_FAILURE);
  }

  /*
   * 1. 去除空格
   * 2. 判断是否是注释或空行
   * 3. 解析配置项，即 key=value
   */
  while (!feof(pf)) {
    char buf[512] = {0};
    fgets(buf, 512, pf); // 读取一行配置项

    std::string read_buf(buf);
    Trim(read_buf); // 1. 去掉字符串前后的空格

    // 2. 判断 # 的注释或者空行
    if (read_buf[0] == '#' || read_buf.empty())
      continue;

    // 3. 解析配置项
    int idx = read_buf.find('=');
    if (idx == -1)
      continue;

    std::string key;
    std::string value;
    key = read_buf.substr(0, idx);
    Trim(key);
    int endidx = read_buf.find('\n', idx);
    value = read_buf.substr(idx + 1, endidx - idx - 1);
    Trim(value);
    m_configMap.insert({key, value});
  }

  fclose(pf);
}

/* 查询配置项信息 */
std::string MprpcConfig::Load(const std::string &key) {
  auto it = m_configMap.find(key);
  if (it == m_configMap.end())
    return "";
  return it->second;
}

/* 去掉字符串前后的空格 */
void MprpcConfig::Trim(std::string &src_buf) {
  int idx = src_buf.find_first_not_of(' '); // 查找字符串中第一个非空格字符
  if (idx != -1)
    src_buf = src_buf.substr(idx, src_buf.size() - idx);

  idx = src_buf.find_last_not_of(' '); // 查找字符串中最后一个非空格字符
  if (idx != -1)
    src_buf = src_buf.substr(0, idx + 1);
}