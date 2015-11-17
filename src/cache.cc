/****************************************************
 *  web proxy cache implementation file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/cache.h"

Cache::~Cache() 
{
  for(it = cache.begin(); it != cache.end(); it++) {
    delete it->second;
  }
}

void Cache::add(string key, CacheEntry * value) 
{
  if (cache.size() > MAX_ENTRIES) {
    lru();
  }
  cache[key] = value;
}

CacheEntry* Cache::get(string key) 
{
  if (cache.find(key) != cache.end()) {
    if (cache[key]->isFresh()) {
      cache[key] -> updateAccessTime();
      return cache[key];
    } else {
      delete cache[key];
      cache.erase(key);
    }
  }
  return NULL;
}

unsigned Cache::size() 
{
  return cache.size();
}

void Cache::lru()
{
  CacheEntry* minEntry = cache.begin()->second;
  string minRequest = cache.begin()->first;
  for(it = cache.begin(); it != cache.end(); it++) {
    if(it->second->getLastAccess() < minEntry->getLastAccess()) {
      minRequest = it->first;
      minEntry = it->second;
    }
  }
  cache.erase(minRequest);
  delete minEntry;
}

// dump to file
bool Cache::dumpToFile(char* fileName)
{
  ofstream file;
  file.open(fileName);
  file << cache.size() << '\n';
  for(it = cache.begin(); it != cache.end(); it++) {
    file << it->first + '\n';
    file << it->second->getCharString() + '\n';
  }
  return true;
}

bool Cache::getFromFile(char* fileName)
{
  ifstream file;
  int size;
  string line;

  file >> size;
  file.open(fileName);

  for(int i = 0;i < size; i++) {
    string currentRequest;

    // get the request string
    while(getline(file, line)) {
      currentRequest+=line;
      if(currentRequest.substr(currentRequest.length()-8) == "\r\n\r\n") {
	break;
      }
    }

    // get the CacheEntry
    string header;
    string body;
    string entryTime;
    string lastAccessTime;

    // get header
    while(getline(file, line)) {
      // inbetween headers and body \r\n\r\n
      header+=line;
      if(header.substr(header.length()-8) == "\r\n\r\n") {
	break;
      }
    }

    // get body
    while(getline(file, line)) {
      body+=line;
      if(body.substr(body.length()-8)
	 =="\r\n\r\n"){
	break;
      }
    }

    // get time
    getline(file, entryTime);
    getline(file, lastAccessTime);
    cache[currentRequest] = NULL;
  }
  return true;
}

CacheEntry* Cache::checkCache(std::string key) 
{
  request_lock.lock();
  CacheEntry * c;

  std::stringstream fmt;
  fmt << "Looking for key  : " << key;
  log->debug(fmt.str());

  c = this->get(key);
  request_lock.unlock();
  return c;
}

void Cache::addToCache(std::string key, CacheEntry * value) 
{
  request_lock.lock();

  std::stringstream fmt;
  fmt << "Adding key  : " << key;
  log->debug(fmt.str());
  
  this->add(key, value);
  request_lock.unlock();
}
