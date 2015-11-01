/****************************************************
 *  web proxy main file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/webproxy.h"
#include "include/logger.h"

#include <memory>
#include <thread>

using namespace webkit;
using namespace diagnostics;

int main(int argc, char *argv[])
{
  std::unique_ptr<abstract_proxy> proxy;
  std::shared_ptr<logger> log = logger::get_logger();

  try {
    log->info("Starting web proxy");
    proxy = std::unique_ptr<abstract_proxy>(new web_proxy);
    proxy->start();
  } catch (...) {
    
  }

  return 0;
}
