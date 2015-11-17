/****************************************************
 *  Test cache
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/cache.h"

#include <memory>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace diagnostics;
static Cache *cache;

int main(int argc, char *argv[])
{

  cache = (Cache*)mmap(NULL, sizeof *cache, PROT_READ | PROT_WRITE, 
		       MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  if (fork() == 0) {
    std::cout << "Forked .... " << std::endl;
    std::string key = "sunil";
    std::string value = "http://w/ww.github.com/";
    CacheEntry* e = new CacheEntry(value.c_str());

    cache->addToCache(key, e);

    std::string key1 = "GET js/uselessweb.js?v=1 HTTP/1.0Host: www.theuselessweb.com";
    std::string value1 = "http://whttp://ww.github.com/sunil";
    CacheEntry* e1 = new CacheEntry(value1.c_str());
    cache->addToCache(key1, e1);

    std::cout << "size " << cache->size() << "\n";

    exit(EXIT_SUCCESS);
  } else {
    wait(NULL);
    //printf("%d\n", *glob_var);
    std::string key1 = "GET js/uselessweb.js?v=1 HTTP/1.0Host: www.theuselessweb.com";
    auto entry =  cache->checkCache(key1);
    if (entry != NULL) {
      std::cout << entry->getCharString() << "\nsize:" << entry->getLength() << std::endl;
    }

    munmap(cache, sizeof *cache);
  }
  return 0;
}
