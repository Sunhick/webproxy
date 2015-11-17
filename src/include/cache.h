/****************************************************
 *  web proxy cache header file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef CACHE
#define CACHE

#include <tr1/unordered_map>
#include <iostream>
#include <pthread.h>
#include <fstream>
#include <thread>
#include <mutex>

#include "logger.h"
#include "cache_entry.h"
#include <time.h>

const int MAX_ENTRIES = 800;
using namespace tr1;
using namespace diagnostics;

// check freshness, remove if not fresh
// dump cache to disk on close
class Cache{
  unordered_map<string, CacheEntry*> cache;
  unordered_map<string, CacheEntry*>::iterator it;

  // used for maintaining the consistency in cache
  std::mutex request_lock;
  std::shared_ptr<logger> log;

 public:
  Cache() {
    log = logger::get_logger();
  }
  ~Cache();

  void add(string key, CacheEntry * value);  
  CacheEntry * get(string key);
  unsigned size();
  void lru();

  // dump to file
  bool dumpToFile(char* fileName);
  bool getFromFile(char* fileName);
  CacheEntry* checkCache(std::string key);
  void addToCache(std::string key, CacheEntry * value);
};

#endif
