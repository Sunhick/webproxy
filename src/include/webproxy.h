/****************************************************
 *  web proxy header file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef WEB_PROXY_H
#define WEB_PROXY_H

#include "logger.h"
#include "cache.h"

#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <list>
#include <sstream>

#include <pthread.h>
#include <signal.h>
#include <sys/select.h>

#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <vector>

#define BACKLOG 32
#define RIO_BUFSIZE 8192

using namespace diagnostics;

namespace webkit {

  typedef struct {
    int rio_fd;                /* descriptor for this internal buf */
    int rio_cnt;               /* unread bytes in internal buf */
    char *rio_bufptr;          /* next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* internal buffer */
  } rio_t;

  class web_proxy;

  struct thread_data  {
    web_proxy* proxy;
    void* data;
  };

  struct dispatcher_data {
    web_proxy* proxy;
    void* data;
  };
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

    // Program constants
    const int MAX_THREADS = 30;
    const int MAX_PENDING = 5;
    const int EXIT_SIGNAL = 2;
    const int OK_CODE = 1;
    const int BAD_CODE = -1;

    // Global data structures
    static queue<int> REQUEST_QUEUE;
    // Synchronization locks
    static sem_t LOGGING_LOCK;
    static pthread_mutex_t REQUEST_QUEUE_LOCK;
    static pthread_mutex_t HTTP_CACHE_LOCK;
    static pthread_cond_t CONSUME_COND;

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

    void initializeThreadPool();
    void initializeRequestQueue();
    void addRequest(int request);
    int removeRequest();
    void clearRequestQueue();
    static void* consumeRequest(void* info);
    static void* dispatcher(void* arg);

  public:
    explicit web_proxy();
    virtual ~web_proxy();
    bool start(int port) override;
    bool stop() override;
    bool abort() override;
  };
}

#endif
