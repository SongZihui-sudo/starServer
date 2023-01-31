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

if [ $XDG_CURRENT_DESKTOP = XFCE ] ; then
    # xubuntu
    echo System: $XDG_CURRENT_DESKTOP
    xfce4-terminal --hide-menubar --hold \
        --tab -T chunk_server1 -e $script_dir/chunk_server1/Chunk_Server --working-directory=$script_dir/chunk_server1  \
        --tab -T chunk_server2 -e 人$script_dir/chunk_server2/Chunk_Server --working-directory=$script_dir/chunk_server2  \
        --tab -T chunk_server3 -e $script_dir/chunk_server3/Chunk_Server --working-directory=$script_dir/chunk_server3  \
        --tab -T chunk_server4 -e $script_dir/chunk_server4/Chunk_Server --working-directory=$script_dir/chunk_server4  \
        --tab -T chunk_server5 -e $script_dir/chunk_server5/Chunk_Server --working-directory=$script_dir/chunk_server5  \
        --tab -T master_server -e $script_dir/master_server/master_server --working-directory=$script_dir/master_server

elif [ $XDG_CURRENT_DESKTOP = Unity ] ; then
    # ubuntu
    echo System: $XDG_CURRENT_DESKTOP
    for i in {1..5}
    do
            gnome-terminal -t chunk_server${i} --working-directory=$script_dir/chunk_server${i} -- bash -c "$script_dir/chunk_server1/Chunk_Server ; bash"

    done
    gnome-terminal -t master_server --working-directory=$script_dir/master_server -- bash -c "$script_dir/master_server/master_server ; bash"

# error
else
    echo This system is not support!
fi