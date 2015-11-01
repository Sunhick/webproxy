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

      std::thread *t = new std::thread(&web_proxy::dispatch_request, this, newfd);
      auto id = t->get_id();
      auto entry = std::pair<std::thread::id, std::thread&&>(id, std::move(*t));

      std::lock_guard<std::mutex> lock(request_lock);
      pending_requests.insert(std::move(entry));

      std::stringstream format;
      format << "Accepted the client! Client Count:" << pending_requests.size();
      log->info(format.str());
    }

    log->warn("Exiting web proxy");

    // Make sure all clients are serviced, before server goes down
    for (auto &thread : pending_requests)
      thread.second.join();
    
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

void web_proxy::dispatch_request(int newfd)
{
  try {
    std::stringstream format;
    format << "Accepted Worker thread(client) socket Id: " << newfd;
    log->debug(format.str());
    char buff[4096];
      
    if (read(newfd, buff, 4096) == -1) {
      log->fatal("Error in reading the request");
      return;
    }

    format.str("");
    format << "Raw request:" << std::endl << buff;
    log->debug(format.str());

    respond(newfd, buff);

    close(newfd);

    // release from the pending_requests
    auto id = std::this_thread::get_id();
    if (this->pending_requests.find(id) != this->pending_requests.end()) {
      std::lock_guard<std::mutex> lock(this->request_lock);
      // figure out how to delete thread t
      // std::thread&& t = std::move(this->pending_requests[id]);
      // delete t;
      this->pending_requests.erase(id);   
    }

    format.str("");
    format << "Closing Worker thread(client) socket Id: " << newfd;
    log->debug(format.str());

  } catch (std::exception& e) {
    std::stringstream format;
    format << "Error occured in client(worker) thread! Reason: " << e.what();
    log->fatal(format.str());
  } catch (...) {
    log->fatal("Error occured in client(worker) thread!");
  }
}

void web_proxy::respond(int newfd, char* buffer)
{
  struct hostent* host;

  struct sockaddr_in host_addr;
  int flag=0,newsockfd1,port=0,sockfd1;
  char t1[300],t2[300],t3[10];
  char* temp=NULL;
   
  sscanf(buffer,"%s %s %s",t1,t2,t3);

  if(((strncmp(t1,"GET",3)==0))&&((strncmp(t3,"HTTP/1.1",8)==0)||(strncmp(t3,"HTTP/1.0",8)==0))&&(strncmp(t2,"http://",7)==0))
    {
      strcpy(t1,t2);
   
      flag=0;
   
      for(unsigned int i=7;i<strlen(t2);i++)
	{
	  if(t2[i]==':')
	    {
	      flag=1;
	      break;
	    }
	}
   
      temp=strtok(t2,"//");
      if(flag==0)
	{
	  port=80;
	  temp=strtok(NULL,"/");
	}
      else
	{
	  temp=strtok(NULL,":");
	}
   
      sprintf(t2,"%s",temp);
      printf("host = %s",t2);
      host=gethostbyname(t2);
   
      if(flag==1)
	{
	  temp=strtok(NULL,"/");
	  port=atoi(temp);
	}
   
   
      strcat(t1,"^]");
      temp=strtok(t1,"//");
      temp=strtok(NULL,"/");
      if(temp!=NULL)
	temp=strtok(NULL,"^]");
      printf("\npath = %s\nPort = %d\n",temp,port);
   
   
      bzero((char*)&host_addr,sizeof(host_addr));
      host_addr.sin_port=htons(port);
      host_addr.sin_family=AF_INET;
      bcopy((char*)host->h_addr,(char*)&host_addr.sin_addr.s_addr,host->h_length);
   
      sockfd1=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
      newsockfd1=connect(sockfd1,(struct sockaddr*)&host_addr,sizeof(struct sockaddr));
      sprintf(buffer,"\nConnected to %s  IP - %s\n",t2,inet_ntoa(host_addr.sin_addr));
      if(newsockfd1<0)
	log->fatal("Error in connecting to remote server");
   
      printf("\n%s\n",buffer);
      //send(newsockfd,buffer,strlen(buffer),0);
      bzero((char*)buffer,sizeof(buffer));
      if(temp!=NULL)
	sprintf(buffer,"GET /%s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",temp,t3,t2);
      else
	sprintf(buffer,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",t3,t2);
 
 
      int n = send(sockfd1,buffer,strlen(buffer),0);
      log->info(std::string(buffer));
      if(n<0)
	log->fatal("Error writing to socket");
      else {
	do
	  {
	    bzero((char*)buffer,500);
	    n=recv(sockfd1,buffer,500,0);
	    if(!(n<=0))
	      send(newfd,buffer,n,0);
	  } while(n>0);
      }
    }
  else
    {
      send(newfd,"400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED",18,0);
    }

}
