#include "../thread.h"
#include "../../common/common.h"

#include <iostream>
#include <string>
#include <vector>

void func()
{
    std::cout << getTime() << std::endl;
}

int main()
{
    std::vector<star::Threading::ptr> thread_pool;
    for (size_t i = 0; i < 5; i++) 
    {
        star::Threading::ptr test(new star::Threading(func, "test" + std::to_string(i)));
        thread_pool.push_back(test);
    }

    return 0;
}