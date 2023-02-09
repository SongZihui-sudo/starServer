#!/bin/bash

# 编译可执行文件
xmake build master_server
xmake build Chunk_Server

echo Demo: Star File system Test Envirment

# 创建文件夹
mkdir master_server
for i in {1..5}
do
    mkdir chunk_server${i}
done

# 拷贝编译完的二进制文件
cp ../../build/linux/x86_64/debug/master_server ./master_server
for i in {1..5}
do
    cp ../../build/linux/x86_64/debug/Chunk_Server ./chunk_server${i}
done

script_dir=$(cd $(dirname $0);pwd)
echo work-directory=$script_dir

# 创建多个终端运行 master server 和 chunk server 的集群

echo System: $XDG_CURRENT_DESKTOP

if [ ${1} == -memcheck ]; then
    # 探测内存泄漏，需要安装 valgrind
    for i in {1..5}
    do
        gnome-terminal -t chunk_server${i} --working-directory=$script_dir/chunk_server${i} -- bash -c  \
            "valgrind --tool=memcheck --leak-check=full --show-reachable=yes --log-file=chunk_server_mem_check${i}.log --trace-children=yes $script_dir/chunk_server${i}/Chunk_Server ; bash"
    done
    gnome-terminal -t master_server --working-directory=$script_dir/master_server -- bash -c    \
        "valgrind --tool=memcheck --leak-check=full --show-reachable=yes --log-file=master_server_mem_check.log --trace-children=yes $script_dir/master_server/master_server ; bash"
else
    # 不探测内存泄漏，直接运行
    for i in {1..5}
    do
        gnome-terminal -t chunk_server${i} --working-directory=$script_dir/chunk_server${i} -- bash -c "$script_dir/chunk_server${i}/Chunk_Server ; bash"
    done
        gnome-terminal -t master_server --working-directory=$script_dir/master_server -- bash -c "$script_dir/master_server/master_server ; bash"
fi



