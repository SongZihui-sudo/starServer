# 修改 linux 默认的 socket 缓冲区大小
echo Reset linux system default socket buffer size

sudo echo "8192 33554432 67108864" > /proc/sys/net/ipv4/tcp_rmem        #tcp收缓冲区的默认值

sudo echo "8192 33554432 67108864" > /proc/sys/net/ipv4/tcp_wmem        #tcp发缓冲区默认值

sudo echo "8192 33554432 67108864"     > /proc/sys/net/ipv4/tcp_mem

echo Finish change