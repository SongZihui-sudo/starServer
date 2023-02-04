# 修改 linux 默认的 socket 缓冲区大小 80mb
echo Reset linux system default socket buffer size

sudo sysctl -w net.core.rmem_max=12288
sudo sysctl -w net.core.wmem_max=12288
sudo sysctl -w net.core.rmem_default=12288
sudo sysctl -w net.core.wmem_default=12288

echo Finish change to 80 MB