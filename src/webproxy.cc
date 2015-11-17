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

      auto pid = fork();
      if(pid == 0) {
	// let child thread service the client
	dispatch_request(newfd);
      } else {
	close(newfd);
	continue;
      }
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
  char http_url[300],http_version[10];

  memset(request, 0, 500);
  recv(clientsockfd, request, 500, 0);
   
  sscanf(request,"%s %s %s",request_type,http_url,http_version);
      
  if(((strncmp(request_type, "GET", 3)==0))
     &&((strncmp(http_version,"HTTP/1.1",8)==0) || (strncmp(http_version,"HTTP/1.0",8)==0))
     &&(strncmp(http_url,"http://",7)==0))  {

    strcpy(request_type,http_url);
    bool port_num_available = false;
   
    for(unsigned int i = 7; i < strlen(http_url); i++) {
      if(http_url[i]==':') {
	port_num_available = true;
	break;
      }
    }
   
    char* temp = strtok(http_url, "//");
    int port = 80;

    if(!port_num_available) temp = strtok(NULL, "/");
    else temp = strtok(NULL,":");
   
    sprintf(http_url, "%s", temp);

    std::stringstream fmt;
    fmt << "Host : " << http_url;
    log->debug(fmt.str());

    struct hostent* host = gethostbyname(http_url);
   
    if(port_num_available) {
      temp=strtok(NULL,"/");
      port=atoi(temp);
    }
   
    strcat(request_type, "^]");
    temp = strtok(request_type, "//");
    temp = strtok(NULL, "/");

    if(temp != NULL) temp = strtok(NULL, "^]");

    fmt.str("");
    fmt << "Path: "<< temp << " Port: " << port;
    log->info(fmt.str());

    struct sockaddr_in host_addr;
    memset((char*)&host_addr, 0, sizeof(host_addr));

    host_addr.sin_port = htons(port);
    host_addr.sin_family = AF_INET;

    bcopy((char*)host->h_addr, (char*)&host_addr.sin_addr.s_addr, host->h_length);
   
    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    serversockfd = connect(serverfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr));

    sprintf(request,"Connected to %s  IP - %s\n",
	    http_url, inet_ntoa(host_addr.sin_addr));

    if(serversockfd < 0)
      log->fatal("Error in connecting to remote server");
   
    log->info(request);
    memset(request, 0, sizeof(request));

    if(temp != NULL)
      sprintf(request,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
	      temp, http_version, http_url);
    else
      sprintf(request,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",
	      http_version, http_url);

    int count = send(serverfd, request, strlen(request), 0);

    if(count < 0) {
      log->fatal("Error writing to socket");
    } else {
      // read the data from server socket and write to the
      // client socket.
      log->info(request);
      int read = 0;
      do {
	memset(request, 0, 500);
	read = recv(serverfd, request, 500, 0);
	if(read > 0)
	  send(clientsockfd, request, read, 0);
      } while(read > 0);
    }

  } else {
    std::string error = "400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED";
    auto dump =
      [request_type, http_url, http_version]() {
      std::stringstream dmp;
      dmp << "\nCONNECTION TYPE: " << request_type
      << " \nHTTP URL: " << http_url
      << "\nHTTP VERSION:" << http_version; 
      return dmp.str();
    };
    std::stringstream fmt;
    dump();
    fmt << error << "\nDUMP:" << dump();
    log->warn(fmt.str());
    send(clientsockfd, error.c_str(), error.size(), 0);
  }

  close(serverfd);
  close(clientsockfd);
  _exit(0);
}
