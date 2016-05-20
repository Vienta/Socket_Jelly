//
//  Jelly.cpp
//  testBoost
//
//  Created by Vienta on 15/9/15.
//  Copyright (c) 2015年 www.vienta.me. All rights reserved.
//

#include "Jelly.h"
#include <boost/thread/thread.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <sstream>
#include "log.h"

using boost::property_tree::ptree;

#define K_RECONNECT_TIMER 6
#define K_CONNECT_TIMEOUT 6
#define K_READ_TIMEOUT 6
#define K_WRITE_TIMEOUT 10
#define K_HEARTBEAT_TIME 6

//util
#include <stdarg.h>  // For va_start, etc.
#include <memory>    // For std::unique_ptr

std::string string_format(const std::string fmt_str, ...) {
    int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
    std::string str;
    std::unique_ptr<char[]> formatted;
    va_list ap;
    while(1) {
        formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
        strcpy(&formatted[0], fmt_str.c_str());
        va_start(ap, fmt_str);
        final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
        va_end(ap);
        if (final_n < 0 || final_n >= n)
            n += abs(final_n - n + 1);
        else
            break;
    }
    return std::string(formatted.get());
}
//protocol
std::string encodeCmdMsg(long reqId, const string &route, std::map<const std::string, std::string> &params)
{
    std::string msg = "<WifiMasage type=\"cmd\"";
    string tmpRoute = ( boost::format(" name=\"%s\"") % route ).str();
    
    map<const std::string, std::string>::iterator itm;
    std::string paramsTmp;
    for (itm = params.begin(); itm != params.end(); itm++) {
        std::string t = (boost::format(" %s=\"%s\"") % itm->first % itm->second).str();
        paramsTmp.append(t);
    }
    
    msg.append(tmpRoute).append(paramsTmp).append( (boost::format(" reqId=\"%d\"") %reqId).str()).append("></WifiMasage>");

    return msg;
}

function<void (std::map<string, string>)> *funcMapValue(std::map<std::string, std::function<void(std::map<string,string> res)>> &map, std::string key) {
    if (map.find(key) != map.end()) {
        return &map.find(key)->second;
    } else {
        return NULL;
    }
}

string *stringMapValue(std::map<std::string, std::string> &map, std::string key)
{
    if (map.find(key) != map.end()) {
        return &map.find(key)->second;
    } else {
        return NULL;
    }
}

void Jelly::processMessage(std::map<std::string, std::string> &msg)
{
    std::string reqId = msg["reqId"];
    long msgId = atol(reqId.c_str());
    
    if (msgId && msgId > 0) {
        function<void (std::map<string,string>)> msgCallback = callbacksMap[reqId];
        if (msgCallback) {
            msgCallback(msg);
            callbacksMap.erase(reqId);
        }
    } else {
        std::string *name = stringMapValue(msg, "name");
        if (name != NULL) {
            std::string route = msg["name"];
            function<void (std::map<string,string>)>* msgCallback = funcMapValue(callbacksMap, route);
            if (msgCallback != NULL) {
                (*msgCallback)(msg);
                if (route == "getfwnumber" || route == "setmobilekey" || route == "updatesys") {
                    callbacksMap.erase(route);
                }
            }
        }
    }
}

void ignoreSIGPIPE()
{
    struct sigaction sa;
    std::memset( &sa, 0, sizeof(sa) );
    sa.sa_handler = SIG_IGN;
    sigaction( SIGPIPE, &sa, NULL);
}

Jelly::Jelly() : mIsConnected(false),mIsClosing(false), mSocket(mIos),mDeadline(mIos), mHeartBeatTimer(mIos), mReconnectTimer(mIos),mHeartBeat("<WifiMasage type=\"cmd\" name=\"appalive\"></WifiMasage>"),reqId(0)
{
    mDelimiter = "\0";
    mDeadline.async_wait(boost::bind(&Jelly::checkDealine, this));
    ignoreSIGPIPE();
}


Jelly::Jelly(const std::string &heartbeat): mIsConnected(false), mIsClosing(false),mSocket(mIos),mDeadline(mIos),mHeartBeatTimer(mIos),mReconnectTimer(mIos),mHeartBeat(heartbeat), reqId(0)
{
    mDelimiter = "\0";
    ignoreSIGPIPE();
}



Jelly::~Jelly()
{
    disconnect();
}

void Jelly::update()
{
    // calls the poll() function to process network messages
    mIos.poll();
}

void Jelly::run()
{
    mIos.run();
}

void Jelly::connect(const std::string &ip, unsigned short port)
{
    try {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);
        
        connect(endpoint);
    } catch (const std::exception &e) {
        std::cout << "Sever exception:" << e.what() << std::endl;
        Log<string>::logToConsole("Jelly connect exception  1");
    }
}

void Jelly::connect(const std::string &ip, unsigned short port, const std::function<void (string)> &callback)
{
    try {
        if (callback) {
            callbacks[_connectCallback] = callback;
        }
        connect(ip, port);
    } catch (const std::exception &e) {
        std::cout << "Sever exception:" << e.what() << std::endl;
        Log<string>::logToConsole("Jelly connect exception 2");
    }
}

void Jelly::disconnect(const std::function<void(string res)>& disconnectCallback)
{
    if (disconnectCallback) {
        callbacks[_disconnectCallback] = disconnectCallback;
    }
}

void Jelly::request(const std::string &route, std::map<const std::string, std::string> &params, const std::function<void (std::map<string,string> map)>& callback)
{
    if (callback) {
        ++reqId;
        std::stringstream ss;
        std::string str;
        ss.str("");
        ss << reqId;
        ss >> str;
        std::string reqKey = str;
        callbacksMap[reqKey] = callback;
        if (route == "getfwnumber" || route == "setmobilekey" || route == "updatesys") {
            callbacksMap[route] = callback;
        }
        
        std::string cmdMsg = encodeCmdMsg(reqId, route, params);
//        Log<string>::logToConsole("Jelly request sendMsg "+route);
        this->sendMsg(cmdMsg);
    } else {
//        Log<string>::logToConsole("Jelly request notifyWithRoute "+route);
        this->notifyWithRoute(route, params);
    }
}

void Jelly::specialRequest(const std::string cmd, const std::string name, const std::function<void (std::map<string, string>)> &callback)
{
    callbacksMap[name] = callback;
    this->sendMsg(cmd);
}

void Jelly::notifyWithRoute(const std::string &route, std::map<const std::string, std::string> &params)
{
    std::string cmdMsg = encodeCmdMsg(0, route, params);
    this->sendMsg(cmdMsg);
}

void Jelly::notifyWithRoute(const std::string &route)
{
    std::map<const string, string> nullParams;
//    this->notifyWithRoute(route, nullParams);
    std::string cmdMsg = encodeCmdMsg(0, route, nullParams);
    this->sendMsg(cmdMsg);
}

//onRoute目前仅支持加入一个callback
void Jelly::onRoute(const std::string &route, const std::function<void (std::map<string, string>)> &callback)
{
    function<void (std::map<string,string>)> *msgCallback = funcMapValue(callbacksMap, route);
    if (msgCallback == NULL) {
        callbacksMap[route] = callback;
    }
//    this->notifyWithRoute(route);
    std::map<const string, string> nullParams;
    std::string cmdMsg = encodeCmdMsg(0, route, nullParams);
    this->sendMsg(cmdMsg);
}

void Jelly::onRoute(const std::string &route, std::map<const std::string, std::string> &params, const std::function<void(std::map<string, string> map)> &callback)
{
    function<void (std::map<string,string>)> *msgCallback = funcMapValue(callbacksMap, route);
    if (msgCallback == NULL) {
        callbacksMap[route] = callback;
    }
//    this->notifyWithRoute(route, params);
    std::string cmdMsg = encodeCmdMsg(0, route, params);
    this->sendMsg(cmdMsg);
}

void Jelly::request(const std::string &route, const std::function<void (std::map<string, string>)> &callback)
{
    Log<string>::logToConsole("Jelly request "+route);
    std::map<const string, string> nullParams;
    this->request(route, nullParams, callback);
}

void Jelly::sendMsg(const std::string &msg)
{
//    std::cout << "send msg:" << msg << std::endl;
    Log<string>::logToConsole("send msg "+msg);
    this->write(msg);
}

void Jelly::connect(boost::asio::ip::tcp::endpoint& endpoint)
{
    if (mIsConnected) {
        return;
    }
    if (mIsClosing) {
        return;
    }
    
    mEndPoint = endpoint;
    
    std::cout << " Trying to connect to port " << endpoint.address().to_string() << ":" << endpoint.port() << std::endl;
    Log<string>::logToConsole("Trying to connect to port "+ endpoint.address().to_string());
    
    mDeadline.expires_from_now(boost::posix_time::seconds(K_CONNECT_TIMEOUT));
    mSocket.async_connect(endpoint, boost::bind(&Jelly::handleConnect, this, boost::asio::placeholders::error));
}

void Jelly::disconnect()
{
    //tell socket to close
    close();
    
    //tell the IO service
    mIos.stop();
    
    mIsConnected = false;
    mIsClosing = false;
    
    std::cout << "disconnect from server" << std::endl;
}

void Jelly::write(const std::string &msg)
{
    if (!mIsConnected) {
        return;
    }
    
    if (mIsClosing) {
        return;
    }
    Log<string>::logToConsole("[Socket]...Send message: "+ msg);
    FILE_LOG(logINFO) << "[Socket]...Send message: " << msg;
    
    mIos.post(boost::bind(&Jelly::doWrite, this, msg));
}

void Jelly::close()
{
    if (!mIsConnected) {
        return;
    }
    
    mIos.post(boost::bind(&Jelly::doClose, this));
}

void Jelly::read()
{
    if (!mIsConnected) {
        return;
    }
    if (mIsClosing) {
        return;
    }
    
    //Set a deadline for the read operation
    mDeadline.expires_from_now(boost::posix_time::seconds(K_READ_TIMEOUT));
    //waiting for message to arrive, the call handle_read
    boost::asio::async_read_until(mSocket, mBuffer, mDelimiter, boost::bind(&Jelly::handleRead, this, boost::asio::placeholders::error));
}

// callbacks
void Jelly::handleConnect(const boost::system::error_code &error) {
    if (mIsClosing) {
        return;
    }
#if ANDROID
    Log<string>::logToConsole("connect result: "+ error.message());
#endif
    std::cout << "connect result:" << error << "error message:" << error.message()<< std::endl;
    if (!error) {
        
        std::cout << "Connect server success:" << error << std::endl;
        
        mIsConnected = true;
        
        // let listeners know
        sConnected(mEndPoint);
        function<void (string res)> connectCallback = callbacks[_connectCallback];
        if (connectCallback) {
//            std::cout << &connectCallback << std::endl;
            connectCallback(_connectSuccess);
        }
        
        // start heartbeat timer (optional)
        mHeartBeatTimer.expires_from_now(std::chrono::seconds(K_HEARTBEAT_TIME));
        mHeartBeatTimer.async_wait(boost::bind(&Jelly::doHeartbeat, this, boost::asio::placeholders::error));

        //await the first message
        read();
    } else {
        mIsConnected = false;
        
        std::cout << "Server error:" << error.message() << std::endl;
        
        function<void (string res)> connectCallBack = callbacks[_connectCallback];
        if (connectCallBack) {
            connectCallBack(_connectFailure);
        }
        
        // schedule a timer to reconnect after  seconds
        mReconnectTimer.expires_from_now(std::chrono::seconds(K_RECONNECT_TIMER));
        mReconnectTimer.async_wait(boost::bind(&Jelly::doReconnect, this, boost::asio::placeholders::error));
    }
}

void Jelly::doWrite(const std::string &msg) {
    if (!mIsConnected) {
        return;
    }
    
    bool writeInProgress = !mMessages.empty();
    mMessages.push_back(msg + mDelimiter);
    
    if (!writeInProgress && !mIsClosing) {
        std::cout << "[Socket...write]...消息真正写入:" << mMessages.front() << std::endl;
        boost::asio::async_write(mSocket, boost::asio::buffer(mMessages.front()), boost::bind(&Jelly::handleWrite, this, boost::asio::placeholders::error));
    } else {
        std::cout << "[Socket...write]...消息未真正写入" << std::endl;
    }
}

void Jelly::doClose() {
    if (mIsClosing) {
        return;
    }
    mIsClosing = true;
    mSocket.close();
}

void Jelly::doHeartbeat(const boost::system::error_code &error) {
    // send a PING and then server replies with a PONG
    if (!error) {
        write(mHeartBeat);
    }
}

void Jelly::handleWrite(const boost::system::error_code &error)
{
//    std::cout << "receive from handle write " << error << std::endl;
    if (!error && !mIsClosing) {
        mMessages.pop_front();
        if (!mMessages.empty()) {
//            std::cout << "Client message:" << mMessages.front() << std::endl;
            boost::asio::async_write(mSocket, boost::asio::buffer(mMessages.front()), boost::bind(&Jelly::handleWrite, this, boost::asio::placeholders::error));
        } else {
            //restart heartbeat timer
            mHeartBeatTimer.expires_from_now(std::chrono::seconds(K_HEARTBEAT_TIME));
            mHeartBeatTimer.async_wait(boost::bind(&Jelly::doHeartbeat,this, boost::asio::placeholders::error));
        }
    }
}

void Jelly::doReconnect(const boost::system::error_code &error) {
    std::cout << "do reconnect" << std::endl;
    if (mIsConnected) {
        std::cout << "~~~~~mIsConnected:" << mIsConnected << std::endl;
        return;
    }
    if (mIsClosing) {
        std::cout << "~~~~~mIsClosing:" << mIsConnected << std::endl;
        return;
    }
    
    mSocket.close();
    //TODO:这里可能有bug
    mMessages.clear();

    mDeadline.expires_from_now(boost::posix_time::seconds(K_RECONNECT_TIMER));
    mSocket.async_connect(mEndPoint, boost::bind(&Jelly::handleConnect,this, boost::asio::placeholders::error));
}

void Jelly::handleRead(const boost::system::error_code &error) {
    
    std::cout << "handle read from server" << std::endl;
    
    if (!error) {
        std::string msg;
        std::istream is(&mBuffer);
        std::getline(is, msg);
        
        if (msg.empty()) {
            return;
        }
        
        std::cout << "[Socket]...Server message: " << msg << std::endl;
        FILE_LOG(logINFO) << "[Socket]...Server message:" << msg;
        
        //解析返回的xml
        std::stringstream ss(msg);
        
        boost::property_tree::ptree pt;

        
        //这里加入try catch的原因是防止msg格式不是xml而导致crash的问题
        try{
            
            boost::property_tree::read_xml(ss, pt);
            
            for (auto& f: pt) {
                string at = f.first + "";
                
                std::cout << f.second.data() << std::endl;
                
                if (at == _callbackMsgProtocal) {
                    
                    if (f.second.data() == _msgSuccess_) {
                        const boost::property_tree::ptree &atrributes = f.second.get_child("<xmlattr>");
                        
                        std::map<std::string, std::string> callbackMsgMap;
                        for (auto& node: atrributes) {
                            string nodeKey = node.first.data();
                            string nodeValue = node.second.data();
                            
                            callbackMsgMap[nodeKey] = nodeValue;
                        }
                        callbackMsgMap["result"] = _msgSuccess_;
                        
                        this->processMessage(callbackMsgMap);
                    } else {
                        const boost::property_tree::ptree &atrributes = f.second.get_child("<xmlattr>");
                        
                        std::map<std::string, std::string> callbackMsgFailMap;
                        for (auto& node: atrributes) {
                            string nodeKey = node.first.data();
                            string nodeValue = node.second.data();
                            
                            callbackMsgFailMap[nodeKey] = nodeValue;
                        }
                        callbackMsgFailMap["result"] = _msgFail_;
                        
                        this->processMessage(callbackMsgFailMap);
                    }
                }
            }
        }
        catch(const boost::property_tree::xml_parser::xml_parser_error& ex){
            std::cout << " xml is not right" << std::endl;
            FILE_LOG(logINFO) << "xml is not right";
        }
        
        //create signal to notify listeners
        sMessage(msg);
        
        //restart heartbeat timerµ
        mHeartBeatTimer.expires_from_now(std::chrono::seconds(5));
        mHeartBeatTimer.async_wait(boost::bind(&Jelly::doHeartbeat, this, boost::asio::placeholders::error));
        
        //wait for the next message
        read();
    } else {
        // try to reconnect if external host disconnects
        std::cout << "disconnect from host " << "err msg:" << error << " message:" << error.message() << std::endl;
        
        
        if (error.value() != 0) {
            mIsConnected = false;
            
            //let listeners know
            sDisconnected(mEndPoint);
            
            //cancel timers
            mHeartBeatTimer.cancel();
            mReconnectTimer.cancel();
            

            function<void (string res)> disconnectCallback = callbacks[_disconnectCallback];
            if (disconnectCallback) {
                disconnectCallback(_disconnectCallback);
            }

            //schedule a timer to reconnect after 5 seconds
            mReconnectTimer.expires_from_now(std::chrono::seconds(K_RECONNECT_TIMER));
            mReconnectTimer.async_wait(boost::bind(&Jelly::doReconnect, this, boost::asio::placeholders::error));
        } else {
            doClose();
        }
    }
}

void Jelly::checkDealine()
{
    if (mIsClosing) {
        return;
    }
    
    std::cout << "deadline expires at: " << mDeadline.expires_at() << " traits_type: " << boost::asio::deadline_timer::traits_type::now() << std::endl;
    FILE_LOG(logINFO) << "deadline expires at: " << mDeadline.expires_at() << " traits_type: " << boost::asio::deadline_timer::traits_type::now();
    
    if (mDeadline.expires_at() <= boost::asio::deadline_timer::traits_type::now()) {
        mHeartBeatTimer.cancel();
        mSocket.close();
        
        mDeadline.expires_at(boost::posix_time::pos_infin);
    }
    mDeadline.async_wait(bind(&Jelly::checkDealine, this));
}

