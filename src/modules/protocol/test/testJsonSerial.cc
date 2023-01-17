#include "../JsonSerial.h"

#include <iostream>
#include <string>

struct testStruct
{
    int a;
    std::string test;
    std::vector< std::string > array_test;
};

int main()
{
    star::JsonSerial::ptr js( new star::JsonSerial );
    testStruct t;
    t.test = "asdawewqe";
    for ( size_t i = 0; i < 5; i++ )
    {
        t.array_test.push_back( std::to_string( i ) );
    }
    t.a = 1;

    /* 结构体序列化为json */
    serialize( js, t.a, t.test, t.array_test );

    Json::Value temp         = js->get();
    Json::Value::Members mem = temp.getMemberNames();
    for ( auto iter = mem.begin(); iter != mem.end(); iter++ )
    {
        std::cout << temp[*iter] << std::endl;
    }

    testStruct t1;
    t1.a    = 200;
    t1.test = "abcdefg";

    /* json在序列化为结构体 */
    deserialize( js, t1.a, t1.test, t1.array_test );

    std::cout << t1.a << std::endl << t1.test << std::endl;

    return 0;
}
