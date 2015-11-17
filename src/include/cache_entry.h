/****************************************************
 *  web proxy cache entry header file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef CACHE_ENTRY_H
#define CACHE_ENTRY_H

#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <time.h>

using namespace std;

class CacheEntry {
 private:
  char * cachedResponse;
  time_t entryTime;
  time_t lastAccessTime;
  int length;

  // Convert time_t object to std::string
  std::string timeToString(time_t t);

  // Convert std::string object to time_t
  time_t stringToTime(std::string ts);
  int parseLength(const char * resp);
  string parseContentType(char * resp);
  void replaceHttpProtocol();

 public:

  CacheEntry(const char * response);
  CacheEntry(char * response, std::string eTime, std::string aTime);
  ~CacheEntry();
  void updateAccessTime();
  char * getCharString();
  int getLength();
  bool isFresh();
  bool isCacheable();
  time_t getLastAccess();
};

#endif
