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

std::string header = R"(
........................................
Welcome to Web proxy v1.0
Author: Sunil <sunhick@gmail.com>
Copyright (c) 2015 All rights reserved.
........................................
)";

int main(int argc, char *argv[])
{
  std::shared_ptr<logger> log = logger::get_logger();
  log->info(header);

  if (argc < 2) {
    log->fatal("Missing port number. \n$./webproxy [port]");
    return 0;
  }

  // look if we wanna enable debug traces
  if (argc == 3) {
    std::string debug(argv[2]);
    if (debug == "-d" || debug == "-debug") {
      log->info("Enabling debug traces");
      log->enable_debug = true;
    }
  }

  try {
    int port = atoi(argv[1]);
    log->info("Starting web proxy on port:" + std::to_string(port));
    std::unique_ptr<abstract_proxy> proxy =
      std::unique_ptr<abstract_proxy>(new web_proxy);
    proxy->start(port);
    proxy->stop();
  } catch (...) {
    log->fatal("Web proxy faulted!");
  }

  log.reset();
  return 0;
}
