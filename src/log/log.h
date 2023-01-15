/**
 * author: SongZihui-sudo
 * file: log.h
 * desc: 日志模块
 * date: 2022-12-24
 */

#ifndef LOG_H
#define LOG_H

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace star
{
class LogFormatter;
class LogManager;

/* 设置日志名 */
#define STAR_NAME( name ) new star::Logger( name )

/* -------------------------------------------------------------------断言 ------------------------------------------------ */
#if defined __GNUC__ || defined __llvm__
/* LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立 */
#define STAR_LIKELY( x ) __builtin_expect( !!( x ), 1 )
/* LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立 */
#define STAR_UNLIKELY( x ) __builtin_expect( !!( x ), 0 )
#else
#define STAR_LIKELY( x ) ( x )
#define STAR_UNLIKELY( x ) ( x )
#endif

#define S(name) \
    std::to_string(name)

/* 断言宏封装 */
#define STAR_ASSERT( x, loggerName )                                                       \
    if ( STAR_UNLIKELY( !( x ) ) )                                                         \
    {                                                                                      \
        ERROR_STD_STREAM_LOG( loggerName ) << "ASSERTION: " #x << "\n"                     \
                                           << "%n%0";                                      \
        assert( x );                                                                       \
    }

/* 断言宏封装 */
#define STAR_ASSERT2( x, w, loggerName )                                                   \
    if ( STAR_UNLIKELY( !( x ) ) )                                                         \
    {                                                                                      \
        ERROR_STD_STREAM_LOG( loggerName ) << "ASSERTION: " #x << "\n" << w << "%n%0";     \
        assert( x );                                                                       \
    }

/* ----------------------------------------------------------------- 流式 std 日志输出宏 ----------------------------------- */
#define DEBUG_STD_STREAM_LOG( LoggerName )                                                 \
    LoggerName->set_appender( star::Logger::Appender::STD );                               \
    LoggerName->set_level( star::LogLevel::level::DEBUG );                                 \
    *( LoggerName ) << "<< DEBUG >> "

#define ERROR_STD_STREAM_LOG( LoggerName )                                                 \
    LoggerName->set_appender( star::Logger::Appender::STD );                               \
    LoggerName->set_level( star::LogLevel::level::ERROR );                                 \
    *( LoggerName ) << "<< ERROR >> "

#define WERN_STD_STREAM_LOG( LoggerName )                                                  \
    LoggerName->set_appender( star::Logger::Appender::STD );                               \
    LoggerName->set_level( star::LogLevel::level::WERN );                                  \
    *( LoggerName ) << "<< WERN >> "

#define DOTKNOW_STD_STREAM_LOG( LoggerName )                                               \
    LoggerName->set_appender( star::Logger::Appender::STD );                               \
    LoggerName->set_level( star::LogLevel::level::DOTKNOW );                               \
    *( LoggerName ) << "<< DOTKNOW >> "

#define INFO_STD_STREAM_LOG( LoggerName )                                                  \
    LoggerName->set_appender( star::Logger::Appender::STD );                               \
    LoggerName->set_level( star::LogLevel::level::INFO );                                  \
    *( LoggerName ) << "<< INFO >> "

#define FATAL_STD_STREAM_LOG( LoggerName )                                                 \
    LoggerName->set_appender( star::Logger::Appender::STD );                               \
    LoggerName->set_level( star::LogLevel::level::FATAL );                                 \
    *( LoggerName ) << "<< FATAL >> "

/* -------------------------------------------------- 流式文件输出宏 ----------------------------------- */
#define DEBUG_FILE_STREAM_LOG( LoggerName )                                                \
    LoggerName->set_appender( star::Logger::Appender::FILE );                              \
    LoggerName->set_level( star::LogLevel::level::DEBUG );                                 \
    *( LoggerName ) << "<< DEBUG >> "

#define ERROR_FILE_STREAM_LOG( LoggerName )                                                \
    LoggerName->set_appender( star::Logger::Appender::FILE );                              \
    LoggerName->set_level( star::LogLevel::level::ERROR );                                 \
    *( LoggerName ) << "<< ERROR >>"

#define WERN_FILE_STREAM_LOG( LoggerName )                                                 \
    LoggerName->set_appender( star::Logger::Appender::FILE );                              \
    LoggerName->set_level( star::LogLevel::level::WERN );                                  \
    *( LoggerName ) << "<< WERN >> "

#define DOTKNOW_FILE_STREAM_LOG( LoggerName )                                              \
    LoggerName->set_appender( star::Logger::Appender::FILE );                              \
    LoggerName->set_level( star::LogLevel::level::DOTKNOW );                               \
    *( LoggerName ) << "<< DOTKNOW >>"

#define INFO_FILE_STREAM_LOG( LoggerName )                                                 \
    LoggerName->set_appender( star::Logger::Appender::FILE );                              \
    LoggerName->set_level( star::LogLevel::level::INFO );                                  \
    *( LoggerName ) << "<< INFO >> "

#define FATAL_FILE_STREAM_LOG( LoggerName )                                                \
    LoggerName->set_appender( star::Logger::Appender::FILE );                              \
    LoggerName->set_level( star::LogLevel::level::FATAL );                                 \
    *( LoggerName ) << "<< FATAL >> "

/* 把所有日志写入文件 */
#define ALL_LOG_TO_FILE( managerName ) managerName.tofile();

/*
    日志级别
*/
class LogLevel
{
public:
    typedef std::shared_ptr< LogLevel > ptr;

    LogLevel(){};

    ~LogLevel(){};

public:
    enum level
    {
        DOTKNOW = 0,
        INFO    = 1,
        WERN    = 2,
        DEBUG   = 3,
        ERROR   = 4,
        FATAL   = 5
    };

public:
    static std::string toString( level in_level );

    static level toLevel( std::string str );
};

static std::map< LogLevel::level, std::string > colorTab = {
/* 输出颜色 */
#define RESET "\033[0m"               /* 重置 */
#define BLACK "\033[30m"              /* 黑色 */
#define RED "\033[31m"                /* 红色 */
#define GREEN "\033[32m"              /* 绿色 */
#define YELLOW "\033[33m"             /* 黄色 */
#define BLUE "\033[34m"               /* 蓝色 */
#define MAGENTA "\033[35m"            /* 品红色 */
#define CYAN "\033[36m"               /* 蓝绿色 */
#define WHITE "\033[37m"              /* 白色 */
#define BOLDBLACK "\033[1m\033[30m"   /* 加粗黑 */
#define BOLDRED "\033[1m\033[31m"     /* 加粗红 */
#define BOLDGREEN "\033[1m\033[32m"   /* 加粗绿 */
#define BOLDYELLOW "\033[1m\033[33m"  /* 加粗黄 */
#define BOLDBLUE "\033[1m\033[34m"    /* 加粗蓝 */
#define BOLDMAGENTA "\033[1m\033[35m" /* 加粗品红 */
#define BOLDCYAN "\033[1m\033[36m"    /* 加粗蓝绿 */
#define BOLDWHITE "\033[1m\033[37m"   /* 加粗白 */
    { LogLevel::level::DEBUG, YELLOW }, { LogLevel::level::ERROR, RED },
    { LogLevel::level::WERN, MAGENTA }, { LogLevel::level::DOTKNOW, CYAN },
    { LogLevel::level::INFO, WHITE },   { LogLevel::level::FATAL, BOLDRED }
};

/*
    日志事件
*/
class LogEvent
{
    friend LogFormatter;

public:
    typedef std::shared_ptr< LogEvent > ptr;

    LogEvent(){};

    LogEvent( LogLevel::level in_level,
              uint32_t in_line,
              std::string in_file,
              uint64_t in_elapse,
              std::string in_threadName,
              uint32_t in_threadId,
              uint32_t in_fiberId,
              uint64_t in_time );

    ~LogEvent(){};

    LogLevel::level getLevel() { return this->m_level; }; /* 获取日志级别 */

    uint32_t get_lineNum() { return this->m_line; }; /* 获取行号 */

    std::string get_fileName() { return this->m_file; }; /* 获取文件名 */

    std::string get_threadName() { return this->m_threadName; }; /* 获取线程名 */

    uint32_t get_threadId() { return this->m_threadId; }; /* 获取线程id */

    uint32_t get_fiberId() { return this->m_fiberId; }; /* 获取协程id */

    uint64_t get_time() { return this->m_time; }; /* 获取时间 */

    std::stringstream& get_formatted() { return this->m_formatted; } /* 获取格式化串 */

    std::string get_detail() { return this->m_message; } /* 获取日志内容 */

    std::string get_name() { return this->m_name; } /* 获取日志名 */

    void set_level( LogLevel::level in_level )
    {
        this->m_level = in_level;
    }; /* 设置日志级别 */

    void set_lineNum( uint32_t in_line ) { this->m_line = in_line; }; /* 设置行号 */

    void set_file( std::string in_file ) { this->m_file = in_file; } /* 设置文件名 */

    void set_threadName( std::string in_threadName )
    {
        this->m_threadName = in_threadName;
    } /* 设置线程名称 */

    void set_threadId( uint32_t in_threadId )
    {
        this->m_threadId = in_threadId;
    } /* 设置线程id */

    void set_fiberId( uint32_t in_fiderId )
    {
        this->m_fiberId = in_fiderId;
    } /* 设置协程id */

    void set_time( uint64_t in_time ) { this->m_time = in_time; } /* 设置日志时间 */

    void set_formatted( std::string str ) { this->m_formatted << str; } /* 设置格式化串 */

    void set_detail( std::string in_message )
    {
        this->m_message = in_message;
    } /* 设置日志内容 */

    void set_logName( std::string LogName ) { this->m_name = LogName; } /* 设置日志名称 */

private:
    LogLevel::level m_level;       /* 日志等级 */
    uint32_t m_line;               /* 行号 */
    std::string m_file;            /* 文件名 */
    std::string m_threadName;      /* 线程名称 */
    uint32_t m_threadId = 0;       /* 线程id */
    uint32_t m_fiberId  = 0;       /* 协程id */
    uint64_t m_time     = 0;       /* 日志时间 */
    std::stringstream m_formatted; /* 格式化串 */
    std::string m_message;         /* 内容 */
    std::string m_name;            /* 日志名称 */
};

/*
    日志格式化器
*/
class LogFormatter
{
public:
    typedef std::shared_ptr< LogFormatter > ptr;

    LogFormatter() { this->m_event.reset( new LogEvent ); };
    ~LogFormatter(){};

    /*
        %T : Tab[\t]
        %t : 线程id
        %N : 线程名称
        %F : 协程id
        %p : 日志级别
        %c : 日志名称
        %f : 文件名
        %l : 行号
        %m : 日志内容
        %n : 换行符[\r\n]
        %D : 时间
        %0 : 结尾
    */
    void format( const char* pattern, std::initializer_list< std::string > arg ); /* 类似与 printf */

    /*
        返回格式
    */
    std::string get_patten() { return this->m_patten.str(); }

    /*
        返回结果
    */
    LogEvent::ptr get_formatted() { return this->m_event; }

    class FormatItem
    {
    public:
        std::shared_ptr< FormatItem > ptr;
    };

private:
    std::ostringstream m_patten; /* 日志格式 */
    bool m_error;                /* 错误标识 */
    LogEvent::ptr m_event;       /* 解析结果 */
};

/*
    tab
*/
class TabItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< TabItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    线程 Id
*/
class ThreadIdItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< ThreadIdItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    线程名称
*/
class ThreadNameItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< ThreadNameItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    协程 Id
*/
class FiberIdItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< FiberIdItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    日志级别
*/
class LevelItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< LevelItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    文件名
*/
class FileItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< FileItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    行号
*/
class LineItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< LineItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    日志名称
*/
class LogNameItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< LogNameItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    日志内容
*/
class MessageItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< MessageItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    换行
*/
class NewLineItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< NewLineItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    时间
*/
class TimeItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< TimeItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    结束
*/
class EndItem : public LogFormatter::FormatItem
{
public:
    typedef std::shared_ptr< TimeItem > ptr;

    static void format( std::ostringstream& in, LogEvent::ptr in_event );
};

/*
    输出日志的位置
*/
class LogAppender
{
    friend LogFormatter;

public:
    typedef std::shared_ptr< LogAppender > ptr;

    LogAppender( LogLevel::level in_level ) { this->m_level = in_level; };

    LogAppender( LogEvent::ptr in_event, LogLevel::level in_level );

    ~LogAppender(){};

public:
    /*
        输出日志 -- 虚函数因为输出位置不同
    */
    virtual void log( LogLevel::level in_level, LogEvent::ptr in_event ) = 0;

    /*
        设置日志的最低级别
    */
    void set_level( LogLevel::level in_level ) { this->m_level = in_level; };

    /*
        获取当前的格式化器
    */
    LogEvent::ptr get_event() { return this->m_event; };

    /*
        获取当前的日志最低级别
    */
    LogLevel::level get_level() { return this->m_level; };

    LogAppender& operator<<( LogEvent::ptr in_event )
    {
        m_event = in_event;
        return *this;
    }

protected:
    LogEvent::ptr m_event;   /* 日志的事件 */
    LogLevel::level m_level; /* 设置输出日志的最低级别 */
};

/*
    输出到控制台 子类
*/
class StdLogAppender : public LogAppender
{
public:
    StdLogAppender( LogLevel::level in_level )
    : LogAppender( in_level ){};

    StdLogAppender( LogEvent::ptr in_event, LogLevel::level in_level )
    : LogAppender( in_event, in_level ){};

public:
    virtual void log( LogLevel::level in_level, LogEvent::ptr in_event ) override;
};

/*
    输出到文件 子类
*/
class FileLogAppender : public LogAppender
{
public:
    FileLogAppender( LogLevel::level in_level )
    : LogAppender( in_level )
    {
        generate_log_file( this->in );
    };

    FileLogAppender( LogEvent::ptr in_event, LogLevel::level in_level )
    : LogAppender( in_event, in_level ){};

    /* 生成一个随即名称的日志文件 */
    static void generate_log_file( std::ofstream& in );

    /* 重新打开日志文件 */
    void reopen();

    /* 设置文件名 */
    void set_name( std::string name ) { this->file_name = name; }

    /* 获取文件名 */
    std::string get_name() { return this->file_name; }

public:
    virtual void log( LogLevel::level in_level, LogEvent::ptr in_event ) override;

private:
    std::ofstream in;
    std::string file_name;
    bool is_open = false;
};

/*
    日志器
*/
class Logger
{
    friend LogManager;

public:
    enum Appender
    {
        STD,
        FILE
    };

public:
    typedef std::shared_ptr< Logger > ptr;

    Logger() { this->m_formatter.reset( new LogFormatter ); };
    Logger( std::string in_name )
    {
        this->set_name( in_name );
        this->m_formatter.reset( new LogFormatter );
    };
    Logger( LogAppender::ptr in_appender, std::string in_name, LogLevel::level in_level )
    {
        this->m_formatter.reset( new LogFormatter );
        this->root    = in_appender;
        this->m_name  = in_name;
        this->m_level = in_level;
    };
    ~Logger(){};

    /*
        设置日志等级
    */
    void set_level( LogLevel::level in_level ) { this->m_level = in_level; }

    /*
        写日志
    */
    void log( LogLevel::level in_level, LogEvent::ptr in_event )
    {
        this->root->log( in_level, in_event );
    };

    /*
        设置日志器名
    */
    void set_name( std::string in_name ) { this->m_name = in_name; }

    void set_appender( LogAppender::ptr in_appender ) { this->root = in_appender; }

    void set_appender( Appender flag, LogLevel::level in_level = LogLevel::level::DOTKNOW )
    {
        switch ( flag )
        {
#define XX( name, bit )                                                                    \
    case Appender::bit:                                                                    \
        this->root.reset( new name( in_level ) );                                          \
        break;

            XX( StdLogAppender, STD );
            XX( FileLogAppender, FILE );

            default:
                std::cout << "<< Appender Error! >>" << std::endl;
                return;

#undef XX
        }
    }

    LogAppender::ptr get_root() { return this->root; }

    /*
        通过流把日志写入 logger
    */
    Logger& operator<<( std::string str )
    {
        const char* temp = str.c_str();
        bool flag        = false;
        while ( *temp )
        {
            if ( flag )
            {
                break;
            }
            else if ( *temp != '%' )
            {
                this->m_formatter->get_formatted()->get_formatted() << *temp;
                temp++;
            }
            else
            {
                temp++;
                switch ( *temp )
                {
                    case 'n':
                        this->m_formatter->get_formatted()->get_formatted() << "\n";
                        temp++;
                        break;
                    case 'b':
                        this->m_formatter->get_formatted()->get_formatted() << "\t";
                        temp++;
                        break;
                    case '0':
                        this->log( m_level, this->m_formatter->get_formatted() );
                        std::string temp1
                        = this->m_formatter->get_formatted()->get_formatted().str();
                        log_list.push_back( temp1 );
                        this->m_formatter->get_formatted()->get_formatted().clear();
                        this->m_formatter->get_formatted()->get_formatted().str( "" ); /* 清空字符串缓冲区 */
                        temp++;
                        flag = true;
                        break;
                }
            }
        }
        return *this;
    };

    /*
        格式化形式把日志写入 logger
    */
    void format( const char* pattern, std::initializer_list< std::string > args )
    {
        this->m_formatter->get_formatted()->get_formatted().clear();
        this->m_formatter->get_formatted()->get_formatted().str( "" ); /* 清空字符串缓冲区 */
        this->m_formatter->format( pattern, args );
        this->log( this->m_formatter->get_formatted()->getLevel(), this->m_formatter->get_formatted() );
        std::string temp = this->m_formatter->get_formatted()->get_formatted().str();
        log_list.push_back( temp );
    };

private:
    std::vector< std::string > log_list; /* 日志输出位置列表 */
    LogAppender::ptr root;               /* 默认 appender */
    LogLevel::level m_level;             /* 日志输出级别 */
    std::string m_name;                  /* 日志器名称 */
    LogFormatter::ptr m_formatter;       /* 格式化器 */
};

/*
    日志管理器
*/
class LogManager
{
public:
    typedef std::shared_ptr< LogManager > ptr;
    LogManager(){};
    ~LogManager(){};

    Logger::ptr find( std::string name ) { return this->m_loggerList[name]; };

    Logger::ptr get_root() { return this->m_root; };

    void add_logger( Logger::ptr in_logger )
    {
        this->m_loggerList[in_logger->m_name] = in_logger;
    }

    void tofile();

private:
    std::map< std::string, Logger::ptr > m_loggerList;
    Logger::ptr m_root;
};

/* 生成随机字符串 */
std::string random_string( size_t len );

} // namespace star

#endif
