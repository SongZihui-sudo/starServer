echo Reset Database

rm -r ./master_server/Master_DB
rm -r ./master_server/master_server
rm -r ./master_server/master_server_mem_check.log

for i in {1..5} 
do
    rm -r ./chunk_server${i}/Chunk_DB
    rm -r ./chunk_server${i}/Chunk_Server
    rm -r ./chunk_server${i}/chunk_server_mem_check${i}.log
done

echo Done!