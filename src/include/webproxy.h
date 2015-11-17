/****************************************************
 *  web proxy header file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef WEB_PROXY_H
#define WEB_PROXY_H

#include "logger.h"
#include "cache.h"

#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <vector>

#define BACKLOG 32

using namespace diagnostics;

namespace webkit {
  // abstract proxy interface
  class abstract_proxy {
  public:
    virtual ~abstract_proxy() { }
    virtual bool start(int port) = 0;
    virtual bool stop() = 0;
    virtual bool abort() = 0;
  };
  
  // web proxy implementation
  // usage {
  // 	...
  // 	abstract_proxy* proxy = new web_proxy;
  // 	int port = 8888;
  // 	proxy->start(port);
  // 	proxy->stop();
  // 	...
  // }
  class web_proxy : public abstract_proxy {
  private:
    int sockfd;
    int listenPort = 5555;

    // http cache 
    Cache http_cache;

    std::shared_ptr<logger> log;

    // open the server socket for incoming connections
    int open_socket(int backlog);
    // Print the message and exit the program
    int die(const char *format, ...);
    // Handle client request in separate thread
    void dispatch_request(int new_fd);
    std::string build_cache_key(std::string host, std::string path);

  public:
    explicit web_proxy();
    virtual ~web_proxy();
    bool start(int port) override;
    bool stop() override;
    bool abort() override;
  };
}

#endif
