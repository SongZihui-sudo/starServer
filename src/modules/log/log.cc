/**
 * author: SongZihui-sudo
 * file: log.cc
 * desc: 日志模块
 * date: 2022-12-24
 */

#include "./log.h"
#include "modules/common/common.h"

#include <cstdarg>
#include <string>

namespace star
{

LogEvent::LogEvent( LogLevel::level in_level,
                    uint32_t in_line,
                    std::string in_file,
                    uint64_t in_elapse,
                    std::string in_threadName,
                    uint32_t in_threadId,
                    uint32_t in_fiberId,
                    uint64_t in_time )
{
    this->m_level      = in_level;
    this->m_line       = in_line;
    this->m_file       = in_file;
    this->m_threadName = in_threadName;
    this->m_threadId   = in_threadId;
    this->m_fiberId    = in_fiberId;
    this->m_time       = in_time;
}

std::string LogLevel::toString( level in_level )
{
    switch ( in_level )
    {
#define XX( name )                                                                         \
    case LogLevel::name:                                                                   \
        return #name;                                                                      \
        break;

        XX( DOTKNOW );
        XX( ERROR );
        XX( WERN );
        XX( DEBUG );
        XX( INFO );
        XX( FATAL );

#undef XX
    }
    return "DOTKNOW";
}

LogLevel::level LogLevel::toLevel( std::string str )
{
#define XX( name, v )                                                                      \
    if ( str == v )                                                                        \
    {                                                                                      \
        return LogLevel::name;                                                             \
    }

    XX( DOTKNOW, "DOTKNOW" );
    XX( ERROR, "ERROR" );
    XX( WERN, "WERN" );
    XX( DEBUG, "DEBUG" );
    XX( INFO, "INFO" );
    XX( FATAL, "FATAL" );

#undef XX

    return LogLevel::DOTKNOW;
}

LogAppender::LogAppender( LogEvent::ptr in_event, LogLevel::level in_level )
{
    this->m_event = in_event;
    this->m_level = in_level;
}

void StdLogAppender::log( LogLevel::level in_level, LogEvent::ptr in_event )
{
    /*
        在控制台输出
    */
    if ( in_level >= this->m_level )
    {
        switch ( in_level )
        {
#define XX( name )                                                                         \
    case LogLevel::name:                                                                   \
        std::cout << colorTab[name] << in_event->get_formatted().str();                    \
        break;

            XX( LogLevel::level::DEBUG );
            XX( LogLevel::level::WERN );
            XX( LogLevel::level::INFO );
            XX( LogLevel::level::ERROR );
            XX( LogLevel::level::DOTKNOW );
            XX( LogLevel::level::FATAL );

#undef XX
            default:
                std::cout << in_event->get_formatted().str();
                break;
        }
    }
}

void FileLogAppender::log( LogLevel::level in_level, LogEvent::ptr in_event )
{
    /*
        输出到文件里
    */
    if ( in_level >= this->m_level )
    {
        std::string str = "";
        switch ( in_level )
        {
#define XX( name, s )                                                                      \
    case LogLevel::name:                                                                   \
        str = s;                                                                           \
        in << str << in_event->get_formatted().str();                                      \
        break;

            XX( LogLevel::level::DEBUG, "<< DEBUG >>" );
            XX( LogLevel::level::WERN, "<< WERN >>" );
            XX( LogLevel::level::INFO, " << INFO >> " );
            XX( LogLevel::level::ERROR, "<< ERROR >>" );
            XX( LogLevel::level::DOTKNOW, "<< DOTKNOW >>" );
            XX( LogLevel::level::FATAL, "<< FATAL >>" );

            default:
                in << "<< DOTKNOW >>" << in_event->get_formatted().str();
                break;

#undef XX
        }
    }
}

void LogFormatter::format( const char* pattern, std::initializer_list< std::string > arg )
{
    /* 可变参数列表 */
    std::vector< std::string > arg_list = arg;

    bool flag_parse = false;
    bool flag_case  = false;

    /* 类似与 printf 的实现 */
    std::ostringstream log_fotmated_Str;

    size_t index = 0;

    while ( *pattern )
    {
        std::ostringstream current;
        m_error = false;
        if ( *pattern != '%' )
        {
            flag_parse = true;
            log_fotmated_Str << *pattern;
            pattern++;
        }
        else
        {
            flag_case = true;
            pattern++;
            uint32_t u32_temp;
            uint64_t u64_temp;
            LogLevel::level level_temp;
            /* 解析 格式串 */
            switch ( *pattern )
            {

                /*                                                                         \
                    %T : Tab[\t]                                                           \
                    %t : 线程id                                                          \
                    %N : 线程名称                                                      \
                    %F : 协程id                                                          \
                    %p : 日志级别                                                      \
                    %c : 日志名称                                                      \
                    %f : 文件名                                                         \
                    %l : 行号                                                            \
                    %m : 日志内容                                                      \
                    %n : 换行符[\r\n]                                                   \
                    %D : 时间                                                            \
                */
                case 'T':
                    TabItem::format( current, m_event );
                    pattern++;
                    break;
                case 't':
                    u32_temp = std::stoul( arg_list[index] );
                    m_event->set_threadId( u32_temp );
                    ThreadIdItem::format( current, m_event );
                    pattern++;
                    break;
                case 'N':
                    m_event->set_threadName( arg_list[index] );
                    ThreadNameItem::format( current, m_event );
                    pattern++;
                    break;
                case 'F':
                    u32_temp = std::stoul( arg_list[index] );
                    m_event->set_fiberId( u32_temp );
                    FiberIdItem::format( current, m_event );
                    pattern++;
                    break;
                case 'p':
                    level_temp = LogLevel::toLevel( arg_list[index] );
                    m_event->set_level( level_temp );
                    LevelItem::format( current, m_event );
                    pattern++;
                    break;
                case 'c':
                    m_event->set_logName( arg_list[index] );
                    LogNameItem::format( current, m_event );
                    pattern++;
                    break;
                case 'f':
                    m_event->set_file( arg_list[index] );
                    FileItem::format( current, m_event );
                    pattern++;
                    break;
                case 'l':
                    u32_temp = std::stoul( arg_list[index] );
                    m_event->set_lineNum( u32_temp );
                    LineItem::format( current, m_event );
                    pattern++;
                    break;
                case 'm':
                    m_event->set_detail( arg_list[index] );
                    MessageItem::format( current, m_event );
                    pattern++;
                    break;
                case 'n':
                    NewLineItem::format( current, m_event );
                    pattern++;
                    break;
                case 'D':
                    u64_temp = std::stoull( arg_list[index] );
                    m_event->set_time( u64_temp );
                    TimeItem::format( current, m_event );
                    pattern++;
                    break;
                case '0':
                    EndItem::format( current, m_event );
                    pattern++;
                    break;
                /* 错误的格式 */
                default:
                {
                    current << "<< Format Error >>"
                            << " " << pattern;
                    this->m_error = true;
                    pattern++;
                }
            }
            log_fotmated_Str << " " << current.str();
            current.clear();
            index++;
        }
    }

    if ( !flag_case && flag_parse )
    {
        m_event->set_detail( log_fotmated_Str.str() );
        MessageItem::format( log_fotmated_Str, m_event );
    }

    m_event->set_formatted( log_fotmated_Str.str() );
}

void TabItem::format( std::ostringstream& in, LogEvent::ptr in_event ) { in << "\b"; }

void ThreadIdItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_threadId();
}

void ThreadNameItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_threadName();
}

void FiberIdItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_fiberId();
}

void LevelItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << "<<"
       << " " << LogLevel::toString( in_event->getLevel() ) << " "
       << ">>";
}

void LogNameItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_name();
}

void FileItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_fileName();
}

void LineItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_lineNum();
}

void MessageItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    in << in_event->get_detail();
}

void NewLineItem::format( std::ostringstream& in, LogEvent::ptr in_event ) { in << "\n"; }

void TimeItem::format( std::ostringstream& in, LogEvent::ptr in_event )
{
    if ( in_event )
    {
        in << in_event->get_time();
    }
    else
    {
        in << std::to_string(getTime()) << " <----> ";
    }
}

void EndItem::format( std::ostringstream& in, LogEvent::ptr in_event ) { in << std::endl; }

std::string random_string( size_t len )
{
    int real_len = rand() % len;
    if ( real_len == 0 )
        return "";
    char* str = ( char* )malloc( real_len );

    for ( int i = 0; i < real_len; ++i )
    {
        switch ( ( rand() % 3 ) )
        {
            case 1:
                str[i] = 'A' + rand() % 26;
                break;
            case 2:
                str[i] = 'a' + rand() % 26;
                break;
            default:
                str[i] = '0' + rand() % 10;
                break;
        }
    }
    str[real_len - 1]  = '\0';
    std::string string = str;
    free( str );
    return string;
}

void FileLogAppender::generate_log_file( std::ofstream& in )
{
    int random_len   = rand() % 100;
    std::string name = random_string( random_len );
    in.open( name );
    in << "<< Star Sever Log File >>\n";
}

void FileLogAppender::reopen()
{
    if ( !in.is_open() )
    {
        in.open( this->file_name );
        return;
    }
    in.close();
    in.open( this->file_name );
}

void LogManager::tofile()
{
    std::ofstream in;
    FileLogAppender::generate_log_file( in );

    for ( auto it : this->m_loggerList )
    {
        for ( auto obj : it.second->log_list )
        {
            in << obj << "\n";
        }
    }
}

}
