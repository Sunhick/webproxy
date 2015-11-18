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

using namespace webkit;

int gSockId;
bool keep_running = true;

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

    // Initialize thread pool
    initializeThreadPool();

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

      addRequest(newfd);

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

  // if (strcmp(request_type, "") == 0 ||
  //     strcmp(http_hostname, "") == 0 ||
  //     strcmp(http_version, "") == 0) {
  //   std::string error = "400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED";
  //   send(clientsockfd, error.c_str(), error.size(), 0);
  //   close(clientsockfd);
  //   return;
  // }

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
      // close(clientsockfd);
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
    if (cacheentry != nullptr) {
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

      CacheEntry* cacheentry = new CacheEntry(data.str().c_str());
      http_cache.addToCache(key, cacheentry);
      int size = http_cache.size();
      fmt.str("");
      fmt << "Entry added! Cache size:" << size;
      //log->debug(fmt.str()); //////////////////////////
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
