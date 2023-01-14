/**
 * author: SongZihui-sudo
 * file: database.cc
 * desc: 数据库模块测试
 * date: 2023-1-11
 */

#include "../database.h"

#include <iostream>
#include <string>

int main()
{
    /* sqlite3 测试 */
    std::cout << "Sqlite3 Test:" << std::endl;
    star::sqlite::ptr test(new star::sqlite("testDB", "./testDB.db"));
    std::string sql = test->format( "%c %s("  \
         "ID INT %p KEY     NOT NULL," \
         "NAME           TEXT    NOT NULL," \
         "AGE            INT     NOT NULL," \
         "ADDRESS        CHAR(%d)," \
         "SALARY         REAL );", "testTable", 50);
    test->open();
    std::cout << sql << "\n";
    test->exec(sql);
    test->close();

    /* leveldb 测试 */
    std::cout << "Leveldb Test:" << std::endl;
    star::levelDB::ptr test2(new star::levelDB("testLevelDB", "./testLevelDB"));
    test2->open();
    test2->Put("1", "test");
    std::string str = "";
    test2->Get("1", str);
    std::cout << str << std::endl;
    test2->Delete("1");
    test2->Get("1", str);
    std::cout << str << std::endl;

    return 0;
}