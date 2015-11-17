/****************************************************
 *  web proxy cache entry implementation file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/cache_entry.h"

// Convert time_t object to std::string
std::string CacheEntry::timeToString(time_t t) {
  char buf[80];
  struct tm * ptr;
  ptr = localtime(&t);
  strftime (buf, 80, "%H:%M:%S, %m/%d/%Y", ptr);
  std::string s;
  for (unsigned int i = 0; i < strlen(buf); i++) {
    s += buf[i];
  }
  return s;
};

// Convert std::string object to time_t
time_t CacheEntry::stringToTime(std::string ts) {

  // Go through ugly code to grab pieces of std::string ts
  int hour, minute, second, year, month, day;
  std::string tmp;
  const char * t = ts.c_str();
  unsigned int i = 0;
  while (i < strlen(t) && t[i] != ':') {
    tmp += t[i++];
  }
  hour = atoi(tmp.c_str());
  tmp = "";
  i++;

  while (i < strlen(t) && t[i] != ':') {
    tmp += t[i++];
  }
  minute = atoi(tmp.c_str());
  tmp = "";
  i++;

  while (i < strlen(t) && t[i] != ',') {
    tmp += t[i++];
  }
  second = atoi(tmp.c_str());
  tmp = "";
  i++;

  while (i < strlen(t) && t[i] != '/') {
    tmp += t[i++];
  }
  month = atoi(tmp.c_str());
  tmp = "";
  i++;

  while (i < strlen(t) && t[i] != '/') {
    tmp += t[i++];
  }
  day = atoi(tmp.c_str());
  tmp = "";
  i++;

  while (i < strlen(t) && t[i] != '/') {
    tmp += t[i++];
  }
  year = atoi(tmp.c_str());

  // Get current local time, update fields, and return new time
  time_t rawTime;
  struct tm * timeInfo;
  time(&rawTime);
  timeInfo = localtime(&rawTime);
  timeInfo->tm_year = year - 1900;
  timeInfo->tm_mon = month - 1;
  timeInfo->tm_mday = day;
  timeInfo->tm_sec = second;
  timeInfo->tm_min = minute;
  timeInfo->tm_hour = hour;
  return mktime(timeInfo);
};

int CacheEntry::parseLength(const char * resp) {
  string response(resp);
  string length;
  string contentLength = "Content-Length: ";
  string breaker = "\r\n\r\n";
  unsigned limit = response.find(breaker);
  unsigned found = response.find(contentLength);
  if (found == std::string::npos || found >= limit) {
    return response.length();
  }
  found += contentLength.length();
  while (response[found] != '\n' && found < response.length())
    length += response[found++];
  found = response.find("\r\n\r\n");
  return found + atoi(length.c_str()); 
}

string CacheEntry::parseContentType(char * resp) {
  string response(resp);
  string content;
  unsigned limit = response.find("\r\n\r\n");
  unsigned found = response.find("Content-Type: ");
  if (found == std::string::npos || found >= limit)
    return "";
  found += 14;
  while (response[found] != '\n')
    content += response[found++];
  return content;
}

void CacheEntry::replaceHttpProtocol() {
  if (length > 10) {
    cachedResponse[7] = '0';
  }
}

CacheEntry::CacheEntry(const char * response) {
  length = parseLength(response);
  cachedResponse = new char[length + 1];
  for (int i = 0; i < length; i++) {
    cachedResponse[i] = response[i];
  }
  cachedResponse[length] = '\0';
  replaceHttpProtocol();
  time(&entryTime);
  time(&lastAccessTime);
}

CacheEntry::CacheEntry(char * response, std::string eTime, std::string aTime) {
  length = parseLength(response);
  cachedResponse = new char[length + 1];
  for (int i = 0; i < length; i++) {
    cachedResponse[i] = response[i];
  }
  entryTime = stringToTime(eTime);
  lastAccessTime = stringToTime(aTime);
}

CacheEntry::~CacheEntry() {
  delete [] cachedResponse;
}

void CacheEntry::updateAccessTime() {
  time(&lastAccessTime);
}

char* CacheEntry::getCharString() {
  return cachedResponse;
}

int CacheEntry::getLength() {
  return length;
}

bool CacheEntry::isFresh() {
  int entrySeconds = entryTime;
  int nowSeconds = time(NULL);
  return nowSeconds - entrySeconds < 10000;
}

bool CacheEntry::isCacheable() {
  string contentType = parseContentType(cachedResponse);
  return contentType.find("text") == std::string::npos;
}

time_t CacheEntry::getLastAccess(){
  return lastAccessTime;
}
