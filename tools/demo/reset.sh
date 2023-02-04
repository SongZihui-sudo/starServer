echo Reset Database

rm -r ./master_server/Master_DB
rm -r ./master_server/master_server

for i in {1..5} 
do
    rm -r ./chunk_server${i}/Chunk_DB
    rm -r ./chunk_server${i}/Chunk_Server
done

echo Done!