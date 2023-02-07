#include "../log.h"

#include <string>

int main()
{
    star::LogManager manager;
    star::Logger::ptr Logger( new star::Logger );
    manager.add_logger( Logger );

    DEBUG_STD_STREAM_LOG( Logger ) << " "
                                   << "Hello World" << star::Logger::endl();

    PRINT_LOG( Logger, "%p %f %l %m %n", "ERROR", __FILE__, __LINE__, "RUN ERROR!" );

    DEBUG_FILE_STREAM_LOG( Logger ) << " "
                                    << "Hello World" << star::Logger::endl();

    FPRINT_LOG( Logger, "%p %f %l %m %n", "ERROR", __FILE__, __LINE__, "RUN ERROR!" );

    return 0;
}