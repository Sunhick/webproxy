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

static const char *user_agent_str = "User-Agent: Mozilla/5.0 \
(X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_str = "Accept: text/html,application/xhtml+xml,\
application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_str = "Accept-Encoding: \
gzip, deflate\r\n";
static const char *connection_str = "Connection: close\r\n";
static const char *proxy_connection_str = "Proxy-Connection: close\r\n";
static const char *http_version_str = "HTTP/1.0\r\n";
/* client_bad_request_str used when host is invalid */
static const char *client_bad_request_str = "HTTP/1.1 400 \
Bad Request\r\nServer: Apache\r\nContent-Length: 140\r\nConnection: \
close\r\nContent-Type: text/html\r\n\r\n<html><head></head><body><p>\
This webpage is not available, because DNS lookup failed.</p><p>\
Powered by Rui Hu and Gao Hao.</p></body></html>";

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

    void job(int newfd);
    int forward_to_server(int fd, int *to_server_fd, char *cache_id,
			  void *cache_content, unsigned int *cache_length);

    int forward_to_client_from_cache(int to_client_fd, void *cache_content,
				     unsigned int cache_length);
    int forward_to_client(int to_client_fd, int to_server_fd);
    /* int forward_to_client_from_cache(int to_client_fd, void *cache_content, */
    /* 				     unsigned int cache_length); */
    int forward_to_client_and_cache(int to_client_fd, int to_server_fd,
				    char *cache_id, void *cache_content);
    void close_fd(int *to_client_fd, int *to_server_fd);
    void rio_readinitb(rio_t *rp, int fd);
    ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
    int parse_request_line(char *buf, char *method, char *protocol,
				      char *host_port, char *resource, char *version);
    void parse_host_port(char *host_port, char *remote_host, char *remote_port);
    int open_clientfd(char *hostname, int port);
    ssize_t rio_writen(int fd, void *usrbuf, size_t n);
    ssize_t Rio_writen(int fd, void *usrbuf, size_t n);
    ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n);
    ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
    void get_size(char *buf, unsigned int *size_pointer);
    ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
    ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);
    int append_content(char *content, unsigned int *content_length,
			      char *buf, unsigned int length);
  public:
    explicit web_proxy();
    virtual ~web_proxy();
    bool start(int port) override;
    bool stop() override;
    bool abort() override;
  };
}

#endif
