#include "../protocol.h"

#include <iostream>
#include <string>

int main()
{
    star::protocol::Protocol_Struct t;
    t.bit  = 1;

    for ( size_t i = 0; i < 5; i++ )
    {
        t.data.push_back(std::to_string(i));
        t.path.push_back(std::to_string(i + 10));
    }
    t.file_name = "xxx";
    t.file_size = 100;
    t.from      = "1231232";

    star::protocol::ptr test( new star::protocol( "test", t ) );

    test->Serialize();
    
    std::cout << test->toStr() << std::endl;
    char* temp = new char[100];
    test->toCStr(temp);
    std::cout << temp << std::endl;
    test->toJson(temp);
    test->display();

    return 0;
}