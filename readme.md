# STAR 分布式文件系统

# 快速开始
## 环境配置
1. 安装 linux 系统（这里以 ubuntu 为例）  
2. 安装 sqlite3 数据库
```shell
sudo apt-get install sqlite3
```
3. 安装 xmake 构建工具
4. 安装编译器 gcc， g++、

## 第三方库

sqlite3 函数库  
```
sudo apt-get install libsqlite3-dev
```
libaco  协程库    
jsoncpp json解析库  
leveldb 数据库  

其他的，在第一次编译时，xmake 会给出安装第三方库的提示，同意安装即可。

# 服务端
## 协议结构体  
```c++
struct Protocol_Struct
{
    int bit;                         /* 标识 */
    std::string from;                /* 请求者ip */
    std::string file_name;           /* 文件名 */
    std::vector< std::string > path; /* 路径 */
    size_t file_size;                /* 大小 */
    std::vector< std::string > data; /* 文件内容 */
    std::vector< void* > customize; /* 自定义内容，内容只可以是原始类型 */
};
```
**序列化与反序列化**  
**序列化**
```c++
/* 序列化 */
#define serialize( obj arg1, ... )                                                        \
    obj->toJson( obj->SplitString( #arg1 + std::string( ", " ) + #__VA_ARGS__ ), arg1, __VA_ARGS__ );
```
由前面的宏，定义了序列化函数的接口，结合 c++ 的模板机制实现一个简单的反射，通过输入结构体中的各个对象来生成与之对应的json格式。

```c++
/* 序列化例子 */
struct testStruct
{
    int a;
    std::string test;
    std::vector< std::string > array_test;
};

serialize( js, t.a, t.test, t.array_test );
```
转换为：
```json
{
    "a": 0, // int
    "test" : "string",  // string
    "array_test": [1,2,3,4,5,6] // std vector
}
```
**反序列化**
```c++
#define deserialize( obj, arg1, ... ) obj->toObj( arg1, __VA_ARGS__ )
```
通过输入结构体中的对象，向反序列化函数传递引用，最后取出相应的值。

```c++
deserialize( js, t1.a, t1.test, t1.array_test );

std::cout << t1.a << std::endl << t1.test << std::endl;

/*
    output:
    0
    string
*/
```  
进一步发展可以根据结构体中对象在内存中的位置，使用指针加上模板的参数，实现反射。  

## 关于响应标识
响应标识也就是指 `Protocol_Struct` 的 bit 位。  
**master server**
```
101 ------ 向客户回复指定的文件的元数据  ---> bit 为 107
|                                                           
106 ------ 向 chunk sever 询问的server上的所有元数据 ---> 102 ------ 接受 chunk-server 发来的 chunk 块的元数据     
|
103 ------ 服务器关闭 ------> 无回复
|
104 ------ 接受用户上传的文件数据（类型采用 const char*）, 根据文件的大小，把文件划分为 chunk ----->  108 |------> 116
|
105 ------ 把系统已经存储的文件名及其路径发给客户端 ------> 109
|
114 ------ chunk server 接受 chunk 成功
|
117 ------ 修改文件路径 ------ > 根据文件块 ------> 向chunk server 发起请求，修改 chunk 所属文件的路径
|
118 ------ 用户认证 ------> 120
|
119 ------ 重新设置用户登录信息 ------> 121
```

**chunk server**
```
106 ------> 102 给 master server 回复 chunk 的数据。
|
110 ------> 111 给 client 回复 chunk 的内容。
|
112 ------> 113 删除 chunk
|
108 ------> 114 添加 chunk
|
115 ------> none client 接受 chunk 成功
```

# 客户端
```
110 ------> 接受 chunk ------> 115  成功
|
112 ------> 删除 chunk ------> 113 删除成功
|
104 ------> 上传 chunk ------> 116 上传成功
|
117 ------> 移动文件 ------ > 121 移动成功
|
103 ------> 关闭 master-server
|
118 ------> 用户认证 ------> 120 成功
|
119 ------> 重新设置用户认证信息 ------> 121 成功
```

# 未来
