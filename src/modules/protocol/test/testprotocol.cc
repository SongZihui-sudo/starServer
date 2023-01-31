#include "../protocol.h"

#include <iostream>
#include <string>

int main()
{
    star::protocol::Protocol_Struct t;
    t.bit = 1;

    t.data         = "test data";
    t.path         = "./root";
    t.file_name    = "xxx";
    t.package_size = 100;
    t.from         = "1231232";

    star::protocol::ptr test( new star::protocol( "test", t ) );

    test->Serialize();

    std::cout << test->toStr() << std::endl;
    const char* temp = new char[100];
    test->toCStr( temp );
    std::cout << temp << std::endl;

    test->toJson( temp );

    test->display();

    star::protocol::Protocol_Struct t2;
    star::protocol::ptr test2( new star::protocol( "test", t2 ) );

    test2->Serialize();

    test2->toJson(
    "{\"bit\": 101, \"from\": \"192.168.0.103\", \"file_name\": \"test\", \"path\": \"root\", "
    "\"data\": \"test\",\"package_size\" : 1000, \"customize\": []}" );

    test2->Deserialize();

    test2->display();

    return 0;
}