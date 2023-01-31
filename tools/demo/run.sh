xmake build master_server
xmake build Chunk_Server

echo Demo: Star File system Test Envirment

cp ../../build/linux/x86_64/debug/master_server ./master_server
cp ../../build/linux/x86_64/debug/Chunk_Server ./chunk_server1
cp ../../build/linux/x86_64/debug/Chunk_Server ./chunk_server2
cp ../../build/linux/x86_64/debug/Chunk_Server ./chunk_server3
cp ../../build/linux/x86_64/debug/Chunk_Server ./chunk_server4
cp ../../build/linux/x86_64/debug/Chunk_Server ./chunk_server5

script_dir=$(cd $(dirname $0);pwd)

if [ $XDG_CURRENT_DESKTOP = XFCE ] ; then
    echo System: $XDG_CURRENT_DESKTOP
    xfce4-terminal --hide-menubar --hold \
        --tab -T master_server -e $script_dir/master_server/master_server --working-directory=$script_dir/master_server \
        --tab -T chunk_server1 -e $script_dir/chunk_server1/Chunk_Server --working-directory=$script_dir/chunk_server1  \
        --tab -T chunk_server2 -e $script_dir/chunk_server2/Chunk_Server --working-directory=$script_dir/chunk_server2  \
        --tab -T chunk_server3 -e $script_dir/chunk_server3/Chunk_Server --working-directory=$script_dir/chunk_server3  \
        --tab -T chunk_server4 -e $script_dir/chunk_server4/Chunk_Server --working-directory=$script_dir/chunk_server4  \
        --tab -T chunk_server5 -e $script_dir/chunk_server5/Chunk_Server --working-directory=$script_dir/chunk_server5
else
    echo This system is not support!
fi