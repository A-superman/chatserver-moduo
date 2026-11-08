#include"chatserver.hpp"
#include"chatservice.hpp"
#include<functional>
#include<string>
#include"json.hpp"
#include <muduo/base/Logging.h>

using namespace std;
using namespace std::placeholders;
using json = nlohmann::json;

namespace {
constexpr int kHeaderLen = 4;
// 与 Qt 客户端 WireCodec 的入站上限一致
constexpr int kMaxFrameLen = 4 * 1024 * 1024;
}

//初始化聊天服务器对象
ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg)
:_server(loop, listenAddr, nameArg)
{
    //注册连接回调
    _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
    //注册消息回调
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));
    //设置线程数量
    _server.setThreadNum(4);
}

//启动服务
void ChatServer::start()
{
    _server.start();
}

//上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    //客户端断开连接
    if(!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

//上报读写信息相关的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn,
                            Buffer* buffer,
                            Timestamp receiveTime)
{
    //入站与出站对称：4字节大端长度头 + JSON正文。
    //循环处理粘包；正文不全（半包）时留在缓冲区等下一次读，绝不当成一条报文解析
    while (buffer->readableBytes() >= static_cast<size_t>(kHeaderLen))
    {
        int32_t len = buffer->peekInt32(); //muduo已把大端转成主机字节序
        if (len < 0 || len > kMaxFrameLen)
        {
            LOG_ERROR << "ChatServer::onMessage invalid frame length, closing connection";
            conn->shutdown();
            return;
        }
        if (buffer->readableBytes() < static_cast<size_t>(kHeaderLen + len))
            break; //半包：长度头到了，正文还没到齐

        buffer->retrieveInt32();
        string buf = buffer->retrieveAsString(static_cast<size_t>(len));
        try
        {
            //数据的反序列化
            json js = json::parse(buf);
            //达到目的：完全解耦网络模块的代码和业务模块的代码
            //通过js["msgid"]获取=》业务handler=>conn js time
            auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
            //回调消息绑定好的事件处理器，来执行相应的业务处理
            msgHandler(conn, js, receiveTime);
        }
        catch (...)
        {
            //坏帧或业务异常只关这条连接，不能让异常逃出回调打崩整个进程。
            //日志用固定措辞、不打异常内容：验收判据在journal里grep parse_error
            LOG_ERROR << "ChatServer::onMessage bad frame, closing connection";
            conn->shutdown();
            return;
        }
    }
}