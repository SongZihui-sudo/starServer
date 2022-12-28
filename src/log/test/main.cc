#include "../log.h"

#include <string>

int main()
{
    star::LogManager manager;
    star::Logger::ptr Logger( new star::Logger );
    manager.add_logger(Logger);

    DEBUG_STD_STREAM_LOG( Logger, DOTKNOW )
    "<< DEBUG >>"
    << " "
    << "Hello World"
    << "%0";

    Logger->set_appender( star::Logger::Appender::STD, star::LogLevel::level::DOTKNOW );
    Logger->format( "%p %f %l %m", { "ERROR", "log.cc", "11", "RUN ERROR!" } );

    DEBUG_FILE_STREAM_LOG( Logger, DOTKNOW )
    "<< DEBUG >>"
    << " "
    << "Hello World"
    << "%0";

    Logger->set_appender( star::Logger::Appender::FILE, star::LogLevel::level::DOTKNOW );
    Logger->format( "%p %f %l %m", { "ERROR", "log.cc", "11", "RUN ERROR!" } );

    manager.flash(star::random_string(100));

    return 0;
}