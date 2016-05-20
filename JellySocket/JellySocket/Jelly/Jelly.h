//
//  Jelly.h
//  testBoost
//
//  Created by Vienta on 15/9/15.
//  Copyright (c) 2015年 www.vienta.me. All rights reserved.
//

#ifndef __testBoost__Jelly__
#define __testBoost__Jelly__

#include <stdio.h>
#include <iostream>
#include <ostream>
#include <string>
#include <deque>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/signals2.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <map>
#include <functional>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>


static std::string _connectCallback = "__connectCallback__";
static std::string _disconnectCallback = "__disconnectCallback__";
static std::string _connectSuccess = "__jelly_connectSuccess__";
static std::string _connectFailure = "__jelly_connectFailure__";
static std::string _callbackMsgProtocal = "WifiMasage";
static std::string _msgSuccess_ = "success";
static std::string _msgFail_ = "fail";

using boost::asio::ip::tcp;
using namespace std;


typedef boost::shared_ptr<class Jelly> JellyRef;

class Jelly: public boost::enable_shared_from_this<Jelly> {
public:

    Jelly();
    Jelly(const std::string &heartbeat);
    
    virtual ~Jelly(void);
  
    virtual void update();
    
    virtual void run();
    
    virtual void write(const std::string &msg);
    
    virtual void connect(const std::string &ip, unsigned short port);
    virtual void connect(boost::asio::ip::tcp::endpoint& endpoint);

    virtual void disconnect();
    
public:
    boost::signals2::signal<void(const boost::asio::ip::tcp::endpoint&)> sConnected;
    boost::signals2::signal<void(const boost::asio::ip::tcp::endpoint&)> sDisconnected;
    
    boost::signals2::signal<void(const std::string&)> sMessage;
    
protected:
    virtual void read();
    virtual void close();
    
    virtual void handleConnect(const boost::system::error_code& error);
    virtual void handleRead(const boost::system::error_code& error);
    virtual void handleWrite(const boost::system::error_code& error);
    
    virtual void doWrite(const std::string &msg);
    virtual void doClose();
    
    virtual void doReconnect(const boost::system::error_code& error);
    virtual void doHeartbeat(const boost::system::error_code& error);
    
protected:
    bool mIsConnected;
    bool mIsClosing;
    
    boost::asio::ip::tcp::endpoint mEndPoint;
    
    boost::asio::io_service mIos;
    boost::asio::ip::tcp::socket mSocket;
    boost::asio::deadline_timer mDeadline;
    
    boost::asio::streambuf mBuffer;
    
    std::deque<std::string> mMessages;
    
    boost::asio::steady_timer mHeartBeatTimer;
    boost::asio::steady_timer mReconnectTimer;
    
    std::string mDelimiter;
    std::string mHeartBeat;
    
    long reqId;

public:

//    boost::shared_ptr<Jelly> f()
//    {
//        return shared_from_this();
//    }
    
    std::map<std::string, std::function<void(string res)> > callbacks;                                                                          //callback dictionary
    
    std::map<std::string, std::function<void(std::map<string,string> res)>> callbacksMap;
    void connect(const std::string &ip, unsigned short port, const std::function<void (string res)>& callback);                                 //有callback的connect方法
    void disconnect(const std::function<void(string res)>& disconnectCallback);
    
    void request(const std::string &route, std::map<const std::string, std::string> &params, const std::function<void (std::map<string,string> map)>& callback); //有callback请求方法
    void request(const std::string &route, const std::function<void(std::map<string, string> map)>& callback);//无参数的请求方法
    
    void notifyWithRoute(const std::string &route, std::map<const std::string, std::string> &params);//无回调的请求
    void notifyWithRoute(const std::string &route);//无回调,无请求参数的请求

    
    void onRoute(const std::string &route, const std::function<void(std::map<string, string> map)>& callback);//注册route
    void onRoute(const std::string &route, std::map<const std::string, std::string> &params,const std::function<void(std::map<string, string> map)> &callback);//带参数的注册route
    
    void sendMsg(const std::string &msg);
    void processMessage(std::map<std::string, std::string> &msg);
    
    void specialRequest(const std::string cmd,const std::string name, const std::function<void (std::map<string, string> map)> &callback);
    
private:

    void checkDealine();
};

#endif /* defined(__testBoost__Jelly__) */
