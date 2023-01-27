#include "../setting.h"

#include <iostream>

int main()
{
    star::Setting_Manageer::ptr sm(new star::Setting_Manageer);
    star::config::ptr cur(new star::config("./setting.json"));

    sm->add_setting("test", cur);

    cur->parse();

    std::cout << cur->get<std::string>("dict")["test"].asString() << std::endl;

    cur->get<std::string>("name") = "changed name";

    std::cout << cur->get<std::string>("name").asString() << std::endl;

    cur->fllush();

    return 0;
}