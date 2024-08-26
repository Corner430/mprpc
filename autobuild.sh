#!/bin/bash

set -e # 当脚本中的命令返回非零状态码时，脚本将立即退出。这有助于在遇到错误时停止脚本执行

rm -rf `pwd`/build/*

cd `pwd`/build && # && 确保只有在前面的命令成功执行后，才会执行下一个命令

    # `..` 表示 cmake 将查找上级目录（即项目根目录）中的 CMakeLists.txt 文件。
    cmake .. &&

    make # 此命令将根据 CMakeLists.txt 文件中的配置，生成目标文件和可执行文件

cd ..

cp -r `pwd`/src/include `pwd`/lib
