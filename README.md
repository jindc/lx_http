# lx_http
一个C语言实现http协议解析模块

使用状态机实现的http协议解析拼装模块。支持异步解析。

测试及用法
  test/test.c是测试文件
  ./build.sh
  
  ./test -h
  
  usage:test [-h] [--mock] [-s] [-c] [--host] [--port] [--uri] [--req_file] [--resp_file]
  
  测试程序支持使用文件io模拟网络异步io进行调试。
  -h 打印帮助信息
  --mock 使用文件io模拟网络io
  -s 以服务器模式测试请求的解析及拼装响应。
  -c 以客户端模式测试拼装请求及解析响应
  --req-file 用于保存请求或从中读取请求的文件
  --resp_file 用于保存响应或从中读取响应。

编译：
    需要lxlib库，https://github.com/jindc/lxlib.git
    ./build.sh
    
作者：德才
email： jindc@163.com
