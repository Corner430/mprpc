#aux_source_directory(. SRC_LIST)
set(SRC_LIST 
    mprpcapplication.cc 
    mprpcconfig.cc 
    rpcheader.pb.cc 
    rpcprovider.cc 
    mprpcchannel.cc
    mprpccontroller.cc
    logger.cc
    zookeeperutil.cc)

add_library(mprpc ${SRC_LIST}) # 生成静态库
target_compile_definitions(mprpc PRIVATE THREADED) # 为mprpc添加宏定义，同步调用需要
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)