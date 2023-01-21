#include "../protocol.h"

#include <iostream>
#include <string>

int main()
{
    star::protocol::Protocol_Struct t;
    t.bit  = 1;

    t.data = "test data";
    for ( size_t i = 0; i < 5; i++ )
    {
        
        t.path.push_back(std::to_string(i + 10));
    }
    t.file_name = "xxx";
    t.package_size = 100;
    t.from      = "1231232";

    star::protocol::ptr test( new star::protocol( "test", t ) );

    test->Serialize();
    
    std::cout << test->toStr() << std::endl;
    const char* temp = new char[100];
    test->toCStr(temp);
    std::cout << temp << std::endl;

    test->toJson(temp);
    
    test->display();

    star::protocol::Protocol_Struct t2;
    star::protocol::ptr test2( new star::protocol( "test", t2 ) );

    test2->toJson("{\"bit\": 101, \"from\": \"127.0.0.1\", \"file_name\": \"test\", \"path\": [\"root\", \"test\"], \"data\": \"test\", \"customize\": []}");

    test2->display();

    return 0;
}