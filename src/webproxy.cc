/****************************************************
 *  web proxy implementation file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/webproxy.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sstream>
#include <exception>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>


#define MAXLINE  8192 
#define MAXBUF   8192
#define MAX_OBJECT_SIZE 102400

using namespace webkit;

int gSockId;
bool keep_running = true;

extern int h_errno;

web_proxy::web_proxy()
{
  // Initialize locking mechanisms
  sem_init(&LOGGING_LOCK, 0, 1);
  pthread_mutex_init(&REQUEST_QUEUE_LOCK, NULL);
  pthread_mutex_init(&HTTP_CACHE_LOCK, NULL);

  log = logger::get_logger();
  log->debug("web proxy initialized");
}

web_proxy::~web_proxy()
{
  log->debug("web proxy shutdown");
  log.reset();
}

bool web_proxy::start(int port)
{
  listenPort = port;

  try {
    
    // open master socket for client to connect
    // a maximum of 32 client can connect simultaneously.
    gSockId = sockfd = open_socket(BACKLOG);

    // unable to open socket.
    if (sockfd < 0)
      return false;

    // Install a signal handler for graceful shut of server
    log->debug("Installing signal handler");
    std::signal(SIGINT, [](int signal) {
	std::cout << "Signal caught. Closing server socket!" << std::endl;
	keep_running = false;
	close(gSockId);
	exit(EXIT_FAILURE);
      });

    log->info("Successfully started web proxy. Listening to client connections...");

    signal(SIGPIPE, SIG_IGN);
    // Initialize thread pool
    // initializeThreadPool();
    pthread_t tid;
    // Accept incoming connections & launch
    // the request thread to service the clients
    while (keep_running) {
      struct sockaddr_in their_addr;
      socklen_t sin_size = sizeof(struct sockaddr_in);
      int newfd;
      if ((newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
	perror("accept");
	continue;
      }

      // addRequest(newfd);
      dispatcher_data* data = new dispatcher_data{ this, (void*)&newfd };
      pthread_create(&tid, NULL, &web_proxy::dispatcher, data);

      // auto pid = fork();
      // if(pid == 0) {
      // 	// let child thread service the client
      // 	dispatch_request(newfd, http_cache);
      // } else {
      // 	close(newfd);
      // 	continue;
      // }
    }

    log->warn("Exiting web proxy");    
  } catch (std::exception& e) {
    log->fatal("Error in server(main) thread! Reason: " + std::string(e.what()));
  } catch (...) {
    log->fatal("Error in server(main) thread!");
  }

  return true;
}


bool web_proxy::stop()
{
  close(sockfd);
  log->info("Successfully stopped web proxy");
  return true;
}

bool web_proxy::abort()
{
  log->info("Web proxy aborted!");
  return true;
}

int web_proxy::open_socket(int backlog)
{
  struct sockaddr_in sin; 	// an Internet endpoint address 
  int sockfd;               	// socket descriptor 

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;

#ifdef USE_IP
  // support for connecting from different host
  sin.sin_addr.s_addr = inet_addr("10.201.30.63");
#else
  // use local host
  sin.sin_addr.s_addr = INADDR_ANY;
#endif

  // Map port number (char string) to port number (int)
  if ((sin.sin_port=htons((unsigned short)this->listenPort)) == 0)
    die("can't get \"%sockfd\" port number\n", this->listenPort);

  // Allocate a socket
  sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
    die("can't create socket: %sockfd\n", strerror(errno));

  // Bind the socket
  if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    std::stringstream format;
    format << "Cannot bind to " << listenPort << " port: "
	   << strerror(errno) << ". Trying other port";
    log->warn(format.str());
    sin.sin_port=htons(0); // request a port number to be allocated by bind
    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      die("can't bind: %sockfd\n", strerror(errno));
    } else {
      int socklen = sizeof(sin);
      if (getsockname(sockfd, (struct sockaddr *)&sin, (socklen_t *)&socklen) < 0)
	die("getsockname: %sockfd\n", strerror(errno));
      std::stringstream format;
      format << "New server port number is " << ntohs(sin.sin_port);
      log->warn(format.str());
    }
  }

  if (listen(sockfd, backlog) < 0)
    die("can't listen on %sockfd port: %sockfd\n", this->listenPort, strerror(errno));

  return sockfd;
}

int web_proxy::die(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  exit(EXIT_FAILURE);  
}

void web_proxy::dispatch_request(int clientsockfd)
{
  int serversockfd, serverfd;
  char request[510];
  char request_type[300];
  char http_hostname[300],http_version[10];

  int size = http_cache.size();
  std::stringstream ft;
  ft.str("");
  ft << "Cache size:" << size;
  // log->info(ft.str());////////////////

  memset(request, 0, 500);
  recv(clientsockfd, request, 500, 0);
   
  sscanf(request,"%s %s %s",request_type,http_hostname,http_version);

  if (strcmp(request_type, "") == 0 ||
      strcmp(http_hostname, "") == 0 ||
      strcmp(http_version, "") == 0) {
    std::string error = "400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED";
    send(clientsockfd, error.c_str(), error.size(), 0);
    close(clientsockfd);
    return;
  }

  if(((strncmp(request_type, "GET", 3) == 0))
     &&((strncmp(http_version,"HTTP/1.1",8) == 0) || (strncmp(http_version,"HTTP/1.0",8) == 0))
     &&(strncmp(http_hostname,"http://",7) == 0))  {

    strcpy(request_type, http_hostname);
    bool port_num_available = false;
   
    for(unsigned int i = 7; i < strlen(http_hostname); i++) {
      if(http_hostname[i]==':') {
	port_num_available = true;
	break;
      }
    }
   
    char* path = strtok(http_hostname, "//");
    int port = 80;

    if(!port_num_available) path = strtok(NULL, "/");
    else path = strtok(NULL,":");

    if (path != NULL)
      sprintf(http_hostname, "%s", path);

    std::stringstream fmt;
    fmt << "Host : " << http_hostname;
    log->debug(fmt.str());

    struct hostent* host = gethostbyname(http_hostname);
    if (host == NULL) {
      close(clientsockfd);
      return;
    }
   
    if(port_num_available) {
      path = strtok(NULL,"/");
      if (path != NULL) port = atoi(path);
    }
   
    strcat(request_type, "^]");
    path = strtok(request_type, "//");
    path = strtok(NULL, "/");

    if(path != NULL) path = strtok(NULL, "^]");

    // if (path == NULL) strcpy(path, "");

    fmt.str("");
    fmt << "Path: "<< path << " Port: " << port;
    // log->info(fmt.str()); ///////////////////

    // look up in the cache
    std::string path_val("");
    if (path != NULL)
      path_val = std::string(path);
    
    std::string key = build_cache_key(http_hostname, path_val);
    fmt.str("");
    fmt << "KEY:" << key;
    log->debug(fmt.str());

    CacheEntry* cacheentry = http_cache.checkCache(key);
    if (cacheentry != nullptr && cacheentry->getCharString() != NULL) {
      // cache hit
      log->fatal("cache hit!");
      char* response = cacheentry->getCharString();
      int length = cacheentry->getLength();

      send(clientsockfd, response, length, 0);      
      close(clientsockfd);
      return;
      // _exit(0);
    }

    log->debug("cache miss");

    struct sockaddr_in host_addr;
    memset((char*)&host_addr, 0, sizeof(host_addr));

    host_addr.sin_port = htons(port);
    host_addr.sin_family = AF_INET;

    // host_addr.sin_addr.s_addr = (in_addr_t*)malloc(sizeof(in_addr_t));
    bcopy((char*)host->h_addr, (char*)&host_addr.sin_addr.s_addr, host->h_length);
   
    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    serversockfd = connect(serverfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr));

    sprintf(request,"Connected to %s  IP - %s\n",
	    http_hostname, inet_ntoa(host_addr.sin_addr));

    if(serversockfd < 0) {
      log->fatal("Error in connecting to remote server");
      close(clientsockfd);
      return;
    }

    log->info(request);
    memset(request, 0, sizeof(request));

    if(path != NULL)
      sprintf(request,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
	      path, http_version, http_hostname);
    else
      sprintf(request,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
	      http_version, http_hostname);

    int count = send(serverfd, request, strlen(request), 0);

    if(count < 0) {
      log->fatal("Error writing to socket");
    } else {
      // read the data from server socket and write to the
      // client socket.
      log->info(request);
      std::stringstream data;

      int read = 0;
      do {
	memset(request, 0, 500);
	read = recv(serverfd, request, 500, 0);
	data << std::string(request);
	if(read > 0)
	  send(clientsockfd, request, read, 0);
      } while(read > 0);

      if (data.str() != "" && std::string(key) != "") {
	CacheEntry* cacheentry = new CacheEntry(data.str().c_str());
	http_cache.addToCache(key, cacheentry);
	int size = http_cache.size();
	fmt.str("");
	fmt << "Entry added! Cache size:" << size;
	//log->debug(fmt.str()); //////////////////////////
      }
    }

  } else {
    std::string error = "400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED";

    auto dump =
      [request_type, http_hostname, http_version]() {
      std::stringstream dmp;
      dmp << "\nCONNECTION TYPE: " << request_type
      << " \nHTTP URL: " << http_hostname
      << "\nHTTP VERSION:" << http_version; 
      return dmp.str();
    };

    std::stringstream fmt;
    dump();
    fmt << error << "\nDUMP:" << dump();
    // log->warn(fmt.str()); ////////////////////////////
    send(clientsockfd, error.c_str(), error.size(), 0);
  }

  close(serverfd);
  close(clientsockfd);
  // _exit(0);
}

std::string web_proxy::build_cache_key(std::string host, std::string path)
{
  //return  "GET " + path + " HTTP/1.0\n" + "Host: " + host + "\r\n\r\n";
  return std::string("GET " + path + " HTTP/1.0" + "Host: " + host);
}

void web_proxy::initializeThreadPool()
{
  for(int i = 0; i < MAX_THREADS; i++) {
    pthread_t tid;
    // Create our thread startup data
    thread_data* tdata = new thread_data();
    tdata->proxy = this;
    tdata->data = nullptr;

    int rc = pthread_create(&tid, NULL, consumeRequest, tdata);
    if (rc) {
      log->fatal("ERROR in ThreadPool initialization");
      exit(BAD_CODE);
    }
  }
}

void web_proxy::initializeRequestQueue()
{

}

void web_proxy::addRequest(int request)
{
  pthread_mutex_lock(&REQUEST_QUEUE_LOCK);
  REQUEST_QUEUE.push(request);
  pthread_cond_signal(&CONSUME_COND);
  pthread_cond_wait(&CONSUME_COND, &REQUEST_QUEUE_LOCK);
  pthread_mutex_unlock(&REQUEST_QUEUE_LOCK);
}

int web_proxy::removeRequest()
{
  pthread_mutex_lock(&REQUEST_QUEUE_LOCK);
  int r = REQUEST_QUEUE.front();
  REQUEST_QUEUE.pop();
  pthread_mutex_unlock(&REQUEST_QUEUE_LOCK);
  return r;
}

void web_proxy::clearRequestQueue()
{
  log->debug("Clearing request queue.");
  while (!REQUEST_QUEUE.empty()) {
    REQUEST_QUEUE.pop();
  }
}
void* web_proxy::consumeRequest(void* info)
{
  thread_data* tdata = static_cast<thread_data*>(info);

  pthread_detach(pthread_self());
  while (true) {
    int sockfd;
    pthread_cond_wait(&CONSUME_COND, &REQUEST_QUEUE_LOCK);
    if (!REQUEST_QUEUE.empty()) {
      sockfd = REQUEST_QUEUE.front();
      REQUEST_QUEUE.pop();
    }
    pthread_cond_signal(&CONSUME_COND);
    tdata->proxy->dispatch_request(sockfd);
  }
  delete tdata;
  return NULL;
}

queue<int> web_proxy::REQUEST_QUEUE;
sem_t web_proxy::LOGGING_LOCK;
pthread_mutex_t web_proxy::REQUEST_QUEUE_LOCK;
pthread_mutex_t web_proxy::HTTP_CACHE_LOCK;
pthread_cond_t web_proxy::CONSUME_COND = PTHREAD_COND_INITIALIZER;

int web_proxy::forward_to_server(int fd, int *to_server_fd, char *cache_id,
                      void *cache_content, unsigned int *cache_length) {
  char buf[MAXLINE], request_buf[MAXLINE];
  char method[MAXLINE], protocol[MAXLINE];
  char host_port[MAXLINE];
  char remote_host[MAXLINE], remote_port[MAXLINE], resource[MAXLINE];
  char version[MAXLINE];
  char origin_request_line[MAXLINE];
  char origin_host_header[MAXLINE];

  int has_user_agent_str = 0, has_accept_str = 0,
    has_accept_encoding_str = 0,
    has_connection_str = 0, has_proxy_connection_str = 0,
    has_host_str = 0;

  rio_t rio_client;

  strcpy(remote_host, "");
  strcpy(remote_port, "80");
  memset(cache_content, 0, MAX_OBJECT_SIZE);

  rio_readinitb(&rio_client, fd);
  if (Rio_readlineb(&rio_client, buf, MAXLINE) == -1) {
    return -1;
  }
  // used incase dns lookup failed
  strcpy(origin_request_line, buf);

  if (parse_request_line(buf, method, protocol, host_port,
			 resource, version) == -1) {
    return -1;
  }
  parse_host_port(host_port, remote_host, remote_port);

  if (strstr(method, "GET") != NULL) {
    // GET method

    // compose our request line
    strcpy(request_buf, method);
    strcat(request_buf, " ");
    strcat(request_buf, resource);
    strcat(request_buf, " ");
    strcat(request_buf, http_version_str);

    // process request header
    while (Rio_readlineb(&rio_client, buf, MAXLINE) != 0) {
      if (strcmp(buf, "\r\n") == 0) {
	break;
      } else if (strstr(buf, "User-Agent:") != NULL) {
	strcat(request_buf, user_agent_str);
	has_user_agent_str = 1;
      } else if (strstr(buf, "Accept-Encoding:") != NULL) {
	strcat(request_buf, accept_encoding_str);
	has_accept_encoding_str = 1;
      } else if (strstr(buf, "Accept:") != NULL) {
	strcat(request_buf, accept_str);
	has_accept_str = 1;
      } else if (strstr(buf, "Connection:") != NULL) {
	strcat(request_buf, connection_str);
	has_connection_str = 1;
      } else if (strstr(buf, "Proxy Connection:") != NULL) {
	strcat(request_buf, proxy_connection_str);
	has_proxy_connection_str = 1;
      } else if (strstr(buf, "Host:") != NULL) {
	strcpy(origin_host_header, buf);
	if (strlen(remote_host) < 1) {
	  // if host not specified in request line
	  // get host from host header
	  sscanf(buf, "Host: %s", host_port);
	  parse_host_port(host_port, remote_host, remote_port);
	}
	strcat(request_buf, buf);
	has_host_str = 1;
      } else {
	strcat(request_buf, buf);
      }
    }
    // if not sent, copy in out headers
    if (has_user_agent_str != 1) {
      strcat(request_buf, user_agent_str);
    }
    if (has_accept_encoding_str != 1) {
      strcat(request_buf, accept_encoding_str);
    }
    if (has_accept_str != 1) {
      strcat(request_buf, accept_str);
    }
    if (has_connection_str != 1) {
      strcat(request_buf, connection_str);
    }
    if (has_proxy_connection_str != 1) {
      strcat(request_buf, proxy_connection_str);
    }
    if (has_host_str != 1) {
      sprintf(buf, "Host: %s:%s\r\n", remote_host, remote_port);
      strcat(request_buf, buf);
    }
    strcat(request_buf, "\r\n");
    if (strcmp(remote_host, "") == 0) {
      return -1;
    }

    // compose cache id
    strcpy(cache_id, method);
    strcat(cache_id, " ");
    strcat(cache_id, remote_host);
    strcat(cache_id, ":");
    strcat(cache_id, remote_port);
    strcat(cache_id, resource);
    strcat(cache_id, " ");
    strcat(cache_id, version);

    // search in the cache
    // if (read_cache_node_lru_sync(cache, cache_id, cache_content,
    //                             cache_length) != -1) {
    //   printf("cache hit.....\n"); 
    //     // cache hit
    //     return 1;
    // }
    CacheEntry* entry = http_cache.checkCache(cache_id);
    if (entry) {
      strcpy(cache_content, (const char*)entry->getCharString());
      // cache_content = entry->getCharString();
      cache_length = (unsigned int*)entry->getLength();
      printf("cache hit...\n");
      return 1;
    }

    printf("cache miss.....\n");
    // client to server
    *to_server_fd = open_clientfd(remote_host, atoi(remote_port));
    if (*to_server_fd == -1) {
      return -1;
    } else if (*to_server_fd == -2) {
      // dns lookup failed, write our response page
      // caused by invalid host
      strcpy(buf, client_bad_request_str);
      Rio_writen(fd, buf, strlen(buf));
      return -1;
    }
    if (Rio_writen(*to_server_fd, request_buf,
		   strlen(request_buf)) == -1) {
      return -1;
    }
    return 0;
  } else {
    // non GET method
    unsigned int length = 0, size = 0;
    strcpy(request_buf, buf);
    while (strcmp(buf, "\r\n") != 0 && strlen(buf) > 0) {
      if (Rio_readlineb(&rio_client, buf, MAXLINE) == -1) {
	return -1;
      }
      if (strstr(buf, "Host:") != NULL) {
	strcpy(origin_host_header, buf);
	if (strlen(remote_host) < 1) {
	  sscanf(buf, "Host: %s", host_port);
	  parse_host_port(host_port, remote_host, remote_port);
	}
      }
      get_size(buf, &size);
      strcat(request_buf, buf);
    }
    if (strcmp(remote_host, "") == 0) {
      return -1;
    }
    *to_server_fd = open_clientfd(remote_host, atoi(remote_port));
    if (*to_server_fd < 0) {
      return -1;
    }

    if (Rio_writen(*to_server_fd, request_buf,
		   strlen(request_buf)) == -1) {
      return -1;
    }
    // write request body
    while (size > MAXLINE) {
      if ((length = Rio_readnb(&rio_client, buf, MAXLINE)) == -1) {
	return -1;
      }
      if (Rio_writen(*to_server_fd, buf, length) == -1) {
	return -1;
      }
      size -= MAXLINE;
    }
    if (size > 0) {
      if ((length = Rio_readnb(&rio_client, buf, size)) == -1) {
	return -1;
      }
      if (Rio_writen(*to_server_fd, buf, length) == -1) {
	return -1;
      }
    }
    return 2;
  }
}

int web_proxy::forward_to_client(int to_client_fd, int to_server_fd) {
  rio_t rio_server;
  char buf[MAXLINE];
  unsigned int length = 0, size = 0;

  rio_readinitb(&rio_server, to_server_fd);
  // forward status line
  if (Rio_readlineb(&rio_server, buf, MAXLINE) == -1) {
    return -1;
  }
  if (Rio_writen(to_client_fd, buf, strlen(buf)) == -1) {
    return -1;
  }
  // forward response headers
  while (strcmp(buf, "\r\n") != 0 && strlen(buf) > 0) {
    if (Rio_readlineb(&rio_server, buf, MAXLINE) == -1) {
      return -1;
    }
    get_size(buf, &size);
    if (Rio_writen(to_client_fd, buf, strlen(buf)) == -1) {
      return -1;
    }
  }
  // forward response body
  if (size > 0) {
    while (size > MAXLINE) {
      if ((length = Rio_readnb(&rio_server, buf, MAXLINE)) == -1) {
	return -1;
      }
      if (Rio_writen(to_client_fd, buf, length) == -1) {
	return -1;
      }
      size -= MAXLINE;
    }
    if (size > 0) {
      if ((length = Rio_readnb(&rio_server, buf, size)) == -1) {
	return -1;
      }
      if (Rio_writen(to_client_fd, buf, length) == -1) {
	return -1;
      }
    }
  } else {
    while ((length = Rio_readnb(&rio_server, buf, MAXLINE)) > 0) {
      if (Rio_writen(to_client_fd, buf, length) == -1) {
	return -1;
      }
    }
  }
  return 0;
}

void web_proxy::rio_readinitb(rio_t *rp, int fd) 
{
  rp->rio_fd = fd;  
  rp->rio_cnt = 0;  
  rp->rio_bufptr = rp->rio_buf;
}

ssize_t web_proxy::rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
  int n, rc;
  char c, *bufp = (char*)usrbuf;

  for (n = 1; n < maxlen; n++) { 
    if ((rc = rio_read(rp, &c, 1)) == 1) {
      *bufp++ = c;
      if (c == '\n')
	break;
    } else if (rc == 0) {
      if (n == 1)
	return 0;
      else
	break;   
    } else
      return -1;	 
  }
  *bufp = 0;
  return n;
}

ssize_t web_proxy::Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
  ssize_t rc;
  if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0){
    if(errno != ECONNRESET && errno != EPIPE){        
      // unix_error("Rio_readlineb error");
    }
  }
  return rc;
} 

int web_proxy::parse_request_line(char *buf, char *method, char *protocol,
				  char *host_port, char *resource, char *version) {
  char url[MAXLINE];
  // check if it is valid buffer
  if (strstr(buf, "/") == NULL || strlen(buf) < 1) {
    return -1;
  }
  // set resource default to '/'
  strcpy(resource, "/");
  sscanf(buf, "%s %s %s", method, url, version);
  if (strstr(url, "://") != NULL) {
    // has protocol
    sscanf(url, "%[^:]://%[^/]%s", protocol, host_port, resource);
  } else {
    // no protocols
    sscanf(url, "%[^/]%s", host_port, resource);
  }
  return 0;
}

void web_proxy::parse_host_port(char *host_port, char *remote_host, char *remote_port) {
  char *tmp = NULL;
  tmp = index(host_port, ':');
  if (tmp != NULL) {
    *tmp = '\0';
    strcpy(remote_port, tmp + 1);
  } else {
    strcpy(remote_port, "80");
  }
  strcpy(remote_host, host_port);
}

int web_proxy::open_clientfd(char *hostname, int port) 
{
  int clientfd;
  struct sockaddr_in serveraddr;
  struct addrinfo *addr_info;

  if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  struct addrinfo hint;
  bzero((void *)&hint, sizeof(hint));
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_family = AF_INET;
  // use getaddrinfo for thread safety
  if(getaddrinfo(hostname, NULL, &hint, &addr_info)){
    return -2;
  }
    
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  serveraddr.sin_addr.s_addr = ((struct sockaddr_in*)(addr_info->ai_addr))->sin_addr.s_addr;
  freeaddrinfo(addr_info);

  if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    return -1;
  return clientfd;
}

ssize_t web_proxy::rio_writen(int fd, void *usrbuf, size_t n) 
{
  size_t nleft = n;
  ssize_t nwritten;
  char *bufp = (char*)usrbuf;

  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
      if (errno == EINTR)  /* interrupted by sig handler return */
	nwritten = 0;    /* and call write() again */
      else
	return -1;       /* errorno set by write() */
    }
    nleft -= nwritten;
    bufp += nwritten;
  }
  return n;
}

ssize_t web_proxy::Rio_writen(int fd, void *usrbuf, size_t n)
{
  ssize_t rc;

  if ((rc = rio_writen(fd, usrbuf, n)) != n){
    if(errno != ECONNRESET && errno != EPIPE){        
      log->fatal("Rio_writen error");
    }
  }
  return rc;
}

ssize_t web_proxy::Rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
  ssize_t rc;

  if ((rc = rio_readnb(rp, usrbuf, n)) < 0){
    if(errno != ECONNRESET && errno != EPIPE){        
      log->fatal("Rio_readnb error");
    }
  }
  return rc;
}

ssize_t web_proxy::rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
  size_t nleft = n;
  ssize_t nread;
  char *bufp =(char*)usrbuf;

  while (nleft > 0) {
    if ((nread = rio_read(rp, bufp, nleft)) < 0) {
      if (errno == EINTR) /* interrupted by sig handler return */
	nread = 0;      /* call read() again */
      else
	return -1;      /* errno set by read() */ 
    } 
    else if (nread == 0)
      break;              /* EOF */
    nleft -= nread;
    bufp += nread;
  }
  return (n - nleft);         /* return >= 0 */
}

void web_proxy::close_fd(int *to_client_fd, int *to_server_fd) 
{
    if (*to_client_fd >= 0) {
        close(*to_client_fd);
    }
    if (*to_server_fd >= 0) {
        close(*to_server_fd);
    }
}

void web_proxy::get_size(char *buf, unsigned int *size_pointer) {
  if (strstr(buf, "Content-Length")) {
    sscanf(buf, "Content-Length: %d", size_pointer);
  }
}

ssize_t web_proxy::rio_read(rio_t *rp, char *usrbuf, size_t n)
{
  int cnt;

  while (rp->rio_cnt <= 0) {  /* refill if buf is empty */
    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
		       sizeof(rp->rio_buf));
    if (rp->rio_cnt < 0) {
      if (errno != EINTR) /* interrupted by sig handler return */
	return -1;
    }
    else if (rp->rio_cnt == 0)  /* EOF */
      return 0;
    else 
      rp->rio_bufptr = rp->rio_buf; /* reset buffer ptr */
  }

  /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
  cnt = n;          
  if (rp->rio_cnt < n)   
    cnt = rp->rio_cnt;
  memcpy(usrbuf, rp->rio_bufptr, cnt);
  rp->rio_bufptr += cnt;
  rp->rio_cnt -= cnt;
  return cnt;
}

int web_proxy::forward_to_client_and_cache(int to_client_fd, int to_server_fd,
                                char *cache_id, void *cache_content) {
  rio_t rio_server;
  char buf[MAXLINE];
  unsigned int cache_length = 0, length = 0, size = 0;
  // if size of response larger than MAX_OBJECT_SIZE
  // valid_obj_size is set to 0
  int valid_obj_size = 1;

  rio_readinitb(&rio_server, to_server_fd);
  // forward status line and write to cache_content
  if (Rio_readlineb(&rio_server, buf, MAXLINE) == -1) {
    return -1;
  }
  if (valid_obj_size) {
    valid_obj_size = append_content((char*)cache_content, &cache_length, buf,
    				    strlen(buf));
  }
  if (Rio_writen(to_client_fd, buf, strlen(buf)) == -1) {
    return -1;
  }
  // forward response headers and write to cache_content
  while (strcmp(buf, "\r\n") != 0 && strlen(buf) > 0) {
    if (Rio_readlineb(&rio_server, buf, MAXLINE) == -1) {
      return -1;
    }
    get_size(buf, &size);
    if (valid_obj_size) {
      valid_obj_size = append_content((char*)cache_content, 
      				      &cache_length, buf, strlen(buf));
    }
    if (Rio_writen(to_client_fd, buf, strlen(buf)) == -1) {
      return -1;
    }
  }
  // forward response body and write to cache_content
  if (size > 0) {
    while (size > MAXLINE) {
      if ((length = Rio_readnb(&rio_server, buf, MAXLINE)) == -1) {
	return -1;
      }
      if (valid_obj_size) {
      	valid_obj_size = append_content((char*)cache_content,
      					&cache_length, buf, length);
      }
      if (Rio_writen(to_client_fd, buf, length) == -1) {
	return -1;
      }
      size -= MAXLINE;
    }
    if (size > 0) {
      if ((length = Rio_readnb(&rio_server, buf, size)) == -1) {
	return -1;
      }
      if (valid_obj_size) {
      	valid_obj_size = append_content((char*)cache_content, &cache_length,
      					buf, length);
      }
      if (Rio_writen(to_client_fd, buf, length) == -1) {
	return -1;
      }
    }
  } else {
    while ((length = Rio_readnb(&rio_server, buf, MAXLINE)) > 0) {
      if (valid_obj_size) {
      	valid_obj_size = append_content((char*)cache_content, &cache_length,
      					buf, length);
      }
      if (Rio_writen(to_client_fd, buf, length) == -1) {
	return -1;
      }
    }
  }
  // write cache_content to cache when size smaller than MAX_OBJECT_SIZE
  if (valid_obj_size) {
    CacheEntry *en = new CacheEntry((const char*)cache_content);
    http_cache.addToCache(cache_id, en);
    // if (add_content_to_cache_sync(cache, cache_id, cache_content,
    // 				  cache_length) == -1) {
    //   return -1;
    // }
  }
  return 0;
}

void* web_proxy::dispatcher(void* arg)
{
  pthread_detach(pthread_self());
  // get file descriptor and free the pointer
  dispatcher_data* tdata = (dispatcher_data*)arg;
  int to_client_fd = *(int*)tdata->data;
  // free(arg);
  tdata->proxy->job(to_client_fd);
  free(arg);
  return NULL;
}

void web_proxy::job(int to_client_fd) 
{
  int to_server_fd = -1;
  int rc = 0;
  char cache_id[MAXLINE];
  char cache_content[MAX_OBJECT_SIZE];
  unsigned int cache_length;

  rc = forward_to_server(to_client_fd, &to_server_fd, cache_id,
			 cache_content, &cache_length);
  if (rc == -1) {
    // some error
    close_fd(&to_client_fd, &to_server_fd);
    pthread_exit(NULL);
  } else if (rc == 1) {
    // found in cache, read from cache
    if (forward_to_client_from_cache(to_client_fd, cache_content,
                                     cache_length) == -1) {
        close_fd(&to_client_fd, &to_server_fd);
        pthread_exit(NULL);
     }    
  } else if (rc == 2) {
    // non GET method, POST etc.
    if (forward_to_client(to_client_fd, to_server_fd) == -1) {
      close_fd(&to_client_fd, &to_server_fd);
      pthread_exit(NULL);
    }
  } else {
    // GET method and write to cache
    if (forward_to_client_and_cache(to_client_fd, to_server_fd, cache_id,
				    cache_content) == -1) {
      close_fd(&to_client_fd, &to_server_fd);
      pthread_exit(NULL);
    }
  }
  close_fd(&to_client_fd, &to_server_fd);
  return;
}

int web_proxy::forward_to_client_from_cache(int to_client_fd, void *cache_content,
					    unsigned int cache_length) {

  log->info("Sending data from cache....");
  log->fatal(std::string((char*)cache_content));
  // forward from cache
  if (Rio_writen(to_client_fd, cache_content, cache_length)) {
    return -1;
  }
  return 0;
}

int web_proxy::append_content(char *content, unsigned int *content_length,
			      char *buf, unsigned int length) {
  if ((*content_length + length) > MAX_OBJECT_SIZE) {
    return 0;
  }
  void *ptr = (void *)((char *)content + *content_length);
  memcpy(ptr, buf, length);
  *content_length = *content_length + length;
  return 1;
}
