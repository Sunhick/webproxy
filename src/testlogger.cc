/****************************************************
 *  Test logger in multi threading
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/logger.h"

#include <memory>
#include <thread>

using namespace diagnostics;

std::shared_ptr<logger> log = logger::get_logger();

void printinfo()
{
  for (int i = 0; i < 50; i++) {
    log->info("this is a info msg");
  }
}

void printwarn()
{
  for (int i = 0; i < 50; i++) {
    log->warn("this is a warn msg");
  }
}


int main(int argc, char *argv[])
{
  std::thread t1(printinfo);
  std::thread t2(printwarn);

  t1.join();
  t2.join();

  return 0;
}
