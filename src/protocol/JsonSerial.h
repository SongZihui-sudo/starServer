/**
 * author: SongZihui-sudo
 * file: byteArray.h
 * desc: 字节数组---实现序列化和反序列化
 * date: 2023-1-13
 */

#include <cstddef>
#include <iterator>
#include <json/json.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace star
{

/* 宏定义 */
/* 序列化 */
#define serialize( obj, arg1, ... )                                                        \
    obj->toJson( obj->SplitString( #arg1 + std::string( ", " ) + #__VA_ARGS__ ), arg1, __VA_ARGS__ );

/* 反序列化 */
#define deserialize( obj, arg1, ... ) obj->toObj( arg1, __VA_ARGS__ )

class JsonSerial
{

    enum type
    {
        Int    = 0,
        UInt   = 1,
        Double = 2,
        String = 3,
        Obj    = 4,
        Array  = 5,
        Bool   = 6,
        Null   = 7
    };

public:
    typedef std::shared_ptr< JsonSerial > ptr;

    JsonSerial()  = default;
    ~JsonSerial() = default;

    // 默认类型为false
    template< typename T, typename... Types >
    struct IsContainerType
    {
        static const bool value = false;
    };

    // Vector类型为true
    template< typename T, typename... Types >
    struct IsContainerType< std::vector< T, Types... > >
    {
        static const bool value = true;
    };

    // deque类型
    template< typename T, typename... Types >
    struct IsContainerType< std::deque< T, Types... > >
    {
        static const bool value = true;
    };

    // set类型
    template< typename T, typename... Types >
    struct IsContainerType< std::set< T, Types... > >
    {
        static const bool value = true;
    };

    // multiset类型
    template< typename T, typename... Types >
    struct IsContainerType< std::multiset< T, Types... > >
    {
        static const bool value = true;
    };

    // map类型
    template< typename K, typename V, typename... Types >
    struct IsContainerType< std::map< K, V, Types... > >
    {
        static const bool value = true;
    };

    // multimap类型
    template< typename K, typename V, typename... Types >
    struct IsContainerType< std::multimap< K, V, Types... > >
    {
        static const bool value = true;
    };

    // 定义获取容器类型的模板
    template< typename T, typename... Types >
    constexpr static bool is_container = IsContainerType< T, Types... >::value;

    /* 对象转json，在进行序列化成二进制数组 */
    /* 可变参数模板和可变参数 */
    template< typename T1, typename... Ts >
    void toJson( std::vector< std::string > names, T1 arg, Ts... args )
    {
        /* 把对象序列化为json格式 */
#define XX( name ) this->m_current[names[this->count]] = name;

        if constexpr ( is_container< T1 > )
        {
            if ( typeid( arg[0] ) == typeid( int ) )
            {
                arr_type.push_back( type::Int );
            }
            else if ( typeid( arg[0] ) == typeid( uint ) )
            {
                arr_type.push_back( type::UInt );
            }
            else if ( typeid( arg[0] ) == typeid( double ) )
            {
                arr_type.push_back( type::Double );
            }
            else if ( typeid( arg[0] ) == typeid( std::string ) )
            {
                arr_type.push_back( type::String );
            }
            else if ( typeid( arg[0] ) == typeid( NULL ) )
            {
                arr_type.push_back( type::Null );
            }
            else if ( typeid( arg[0] ) == typeid( bool ) )
            {
                arr_type.push_back( type::Bool );
            }
            for ( int i = 0; i < arg.size(); i++ )
            {
                this->m_current[names[this->count]][i] = arg[i];
            }

            arr_postion++;
        }
        else if constexpr ( std::is_pointer< T1 >() )
        {
            int i = 0;

            if ( typeid( arg[0] ) == typeid( int ) )
            {
                arr_type.push_back( type::Int );
            }
            else if ( typeid( arg[0] ) == typeid( uint ) )
            {
                arr_type.push_back( type::UInt );
            }
            else if ( typeid( arg[0] ) == typeid( double ) )
            {
                arr_type.push_back( type::Double );
            }
            else if ( typeid( arg[0] ) == typeid( std::string ) )
            {
                arr_type.push_back( type::String );
            }
            else if ( typeid( arg[0] ) == typeid( NULL ) )
            {
                arr_type.push_back( type::Null );
            }
            else if ( typeid( arg[0] ) == typeid( bool ) )
            {
                arr_type.push_back( type::Bool );
            }

            while ( *arg )
            {
                this->m_current[names[this->count]][i] = *arg;
                arg++;
                i++;
            }

            arr_postion++;
        }
        else
        {
            XX( arg );
        }

#undef XX
        /* 如果还有参数，就继续递归 */
        if constexpr ( sizeof...( args ) > 0 )
        {
            judgeType< T1 >( names );
            this->count++;
            toJson( names, args... );
        }

        this->mem       = m_current.getMemberNames();
        current_postion = 0;
        return;
    }

    /* 二进制数字转成json，在转成对象 */
    template< typename T1, typename... Ts >
    void toObj( T1& arg, Ts&... args )
    {
        if ( this->m_name_type.empty() )
        {
            return;
        }

        judgeType1( arg );

        /* 如果还有参数，就继续递归 */
        if constexpr ( sizeof...( args ) > 0 )
        {
            this->current_postion++;
            toObj( args... );
        }
    }

    std::vector< std::string > SplitString( std::string str )
    {

        std::vector< std::string > ret;
        std::string temp;
        bool flag = false;
        int i     = 0;
        while ( i < str.size() )
        {
            if ( str[i] == ',' )
            {
                flag = false;
                i++;
                if ( str[i] == ' ' )
                {
                    ret.push_back( temp );
                    temp.clear();
                    i++;
                }
            }
            else if ( str[i] == '.' )
            {
                i++;
                flag = true;
                temp.push_back( str[i] );
                i++;
            }
            else if ( flag )
            {
                temp.push_back( str[i] );
                i++;
            }
            else
            {
                i++;
            }
        }

        if ( !temp.empty() )
        {
            ret.push_back( temp );
        }
        return ret;
    }

    template< typename T1 >
    void judgeType( std::vector< std::string > names )
    {
        if ( std::is_same_v< T1, int > )
        {
            m_name_type[names[this->count]] = type::Int;
        }
        else if ( std::is_same_v< T1, uint > )
        {
            m_name_type[names[this->count]] = type::UInt;
        }
        else if ( std::is_same_v< T1, double > )
        {
            m_name_type[names[this->count]] = type::Double;
        }
        else if ( std::is_same_v< T1, std::string > )
        {
            m_name_type[names[this->count]] = type::String;
        }
        else if ( std::is_same_v< T1, decltype( NULL ) > )
        {
            m_name_type[names[this->count]] = type::Null;
        }
        else if ( std::is_same_v< T1, bool > )
        {
            m_name_type[names[this->count]] = type::Bool;
        }
        else if ( std::is_pointer< T1 >() )
        {
            m_name_type[names[this->count]] = type::Array;
            auto temp                       = m_name_type[names[this->count]];
            judgeType< decltype( temp ) >( names );
        }
        else
        {
            m_name_type[names[this->count]] = type::Obj;
        }

        m_names = names;
    }

    template< typename T1 >
    void judgeType1( T1& arg )
    {
        if constexpr ( is_container< T1 > )
        {
            size_t sz = m_current[m_names[current_postion]].size();

            for ( int i = 0; i < sz; i++ )
            {
                if constexpr ( std::is_same_v< typename T1::value_type, int > )
                {
                    arg.push_back( this->m_current[m_names[current_postion]][i].asInt() );
                }
                else if constexpr ( std::is_same_v< typename T1::value_type, uint > )
                {
                    arg.push_back( this->m_current[m_names[current_postion]][i].asUInt() );
                }
                else if constexpr ( std::is_same_v< typename T1::value_type, double > )
                {
                    arg.push_back( this->m_current[m_names[current_postion]][i].asDouble() );
                }
                else if constexpr ( std::is_same_v< typename T1::value_type, bool > )
                {
                    arg.push_back( this->m_current[m_names[current_postion]][i].asBool() );
                }
                else if constexpr ( std::is_same_v< typename T1::value_type, decltype( NULL ) > )
                {
                    arg.push_back( this->m_current[m_names[current_postion]][i].asInt() );
                }
                else if constexpr ( std::is_same_v< typename T1::value_type, std::string > )
                {
                    arg.push_back( this->m_current[m_names[current_postion]][i].asString() );
                }
            }
        }
        else if constexpr ( std::is_pointer< T1 >() )
        {
            size_t sz = m_current[m_names[current_postion]].size();
            for ( int i = 0; i < sz; i++ )
            {
                if constexpr ( std::is_same_v< decltype( arg[0] ), int > )
                {
                    arg[i] = this->m_current[m_names[current_postion]][i].asInt();
                }
                else if constexpr ( std::is_same_v< decltype( arg[0] ), uint > )
                {
                    arg[i] = this->m_current[m_names[current_postion]][i].asUInt();
                }
                else if constexpr ( std::is_same_v< decltype( arg[0] ), double > )
                {
                    arg[i] = this->m_current[m_names[current_postion]][i].asDouble();
                }
                else if constexpr ( std::is_same_v< decltype( arg[0] ), bool > )
                {
                    arg[i] = this->m_current[m_names[current_postion]][i].asBool();
                }
                else if constexpr ( std::is_same_v< decltype( arg[0] ), decltype( NULL ) > )
                {
                    arg[i] = this->m_current[m_names[current_postion]][i].asInt();
                }
                else if constexpr ( std::is_same_v< decltype( arg[0] ), std::string > )
                {
                    arg[i] = this->m_current[m_names[current_postion]][i].asString();
                }
            }
        }
        else if constexpr ( std::is_same_v< T1, int > )
        {
            arg = this->m_current[m_names[current_postion]].asInt();
        }
        else if constexpr ( std::is_same_v< T1, uint > )
        {
            arg = this->m_current[m_names[current_postion]].asUInt();
        }
        else if constexpr ( std::is_same_v< T1, double > )
        {
            arg = this->m_current[m_names[current_postion]].asDouble();
        }
        else if constexpr ( std::is_same_v< T1, bool > )
        {
            arg = this->m_current[m_names[current_postion]].asBool();
        }
        else if constexpr ( std::is_same_v< T1, decltype( NULL ) > )
        {
            arg = this->m_current[m_names[current_postion]].asInt();
        }
        else if constexpr ( std::is_same_v< T1, std::string > )
        {
            arg = this->m_current[m_names[current_postion]].asString();
        }
        return;
    }

    void set( Json::Value val ) { this->m_current = val; }
    /* 获取当前的json对象 */
    Json::Value get() { return this->m_current; }

protected:
    Json::Value::Members mem = m_current.getMemberNames();
    size_t current_postion   = 0;
    std::map< std::string, type > m_name_type; /* 变量名称 */
    std::vector< std::string > m_names;
    std::vector< type > arr_type;
    size_t arr_postion;

private:
    Json::Value m_current; /* 当前的json对象 */
    size_t count = 0;      /* 计数 */
};

}