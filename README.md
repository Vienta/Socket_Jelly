# JellyDemo
基于boost库封装的一个socket库,我在一篇[blog中](http://www.vienta.me/2016/03/28/%E5%9F%BA%E4%BA%8Eboost%E5%BA%93%E5%B0%81%E8%A3%85%E7%9A%84%E4%B8%80%E4%B8%AA%E8%B7%A8%E5%B9%B3%E5%8F%B0socket%E5%BA%93/)有介绍

# About Jelly
`Jelly`是我在实际项目实践中实现的一个跨平台的socket解决方案，它是基于C++非常著名的Boost库开发而来，非常轻量。

要成功的跑出demo，需要先到[https://github.com/faithfracture/Apple-Boost-BuildScript](https://github.com/faithfracture/Apple-Boost-BuildScript) ，按照提示跑脚本生成`boost.framework`库，引入到demo中即可

# Usage
  `Jelly`的用法非常简单，如下：

    Jelly* jelly = new Jelly();
    
    jelly->connect("192.168.222.254", 34952, [&](string res) -> void {
        //连接结果
        if (res == _connectSuccess) {
            NSLog(@"连接成功");
        } else {
            NSLog(@"连接失败");
        }
    });
    
    jelly->disconnect([&](string disconnectMsg) -> void {
        //Socket断开的回调
    });
    
    //注册方式，是否带参数可选
    jelly->onRoute(routeName, params, [=](std::map<string, string> res) -> void {
      //res为返回的结果
      if (res["result"] == _msgSuccess_) {
            
      } else {
            
      }
    });
    
    //请求方式，是否带参数可选
    jelly->request(hostname, [=](std::map<string, string> res) -> void {
        //res为返回的结果
        if (res["result"] == _msgSuccess_) {
            
        } else {
          
        }
    });

#  Supported platforms 
理论支持iOS和Android所有设备
