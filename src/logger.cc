/****************************************************
 *  logger implementation file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/logger.h"

#include <string>

using namespace diagnostics;

std::shared_ptr<logger> logger::instance = nullptr;

logger::logger()
{
  colors.insert(std::pair<log_level, modifier>
  		(log_level::DEBUG, modifier(color::FG_BLUE)));
  colors.insert(std::pair<log_level, modifier>
  		(log_level::INFO, modifier(color::FG_GREEN)));
  colors.insert(std::pair<log_level, modifier>
  		(log_level::FATAL, modifier(color::FG_RED)));
  colors.insert(std::pair<log_level, modifier>
  		(log_level::DEFAULT, modifier(color::FG_DEFAULT)));
  colors.insert(std::pair<log_level, modifier>
  		(log_level::WARNING, modifier(color::FG_CYAN)));
  debug("logger initialized");
}

logger::~logger() 
{
  debug("logger shutdown");
}

std::shared_ptr<logger> logger::get_logger()
{
  if (!instance) {
    instance = std::make_shared<logger>();
  }

  return instance;
}

void logger::log(log_level level, const std::string& msg)
{
  writer.lock();
  auto enumstr = [](log_level ll) {
    switch(ll) {
    case log_level::DEBUG: return "[DEBUG]";
    case log_level::INFO: return "[INFO]";
    case log_level::FATAL: return "[FATAL]";
    case log_level::WARNING: return "[WARN]";
    default: return "[DEFAULT]";
    }  
  };
  std::string type = enumstr(level);
  std::cout << colors.at(level) << type << " " << msg << colors.at(log_level::DEFAULT) << std::endl;
  writer.unlock();
}

void logger::info(const std::string& msg)
{
  log(log_level::INFO, msg);
}

void logger::debug(const std::string& msg)
{
  if (enable_debug)
    log(log_level::DEBUG, msg);
}

void logger::warn(const std::string& msg)
{
  log(log_level::WARNING, msg);
}

void logger::fatal(const std::string& msg)
{
  log(log_level::FATAL, msg);
}

// template<typename T, typename... Args>
// void logger::expand(std::ostream& ostream, T msg, Args... args)
// {
//   ostream << msg;
//   expand(ostream, args...);
// }

// template<typename... Args>
// void logger::info(Args... args)
// {
//   std::ostringstream oss;
//   expand(oss, args...);
//   log(log_level::INFO, oss.str());
// }

// template<typename... Args>
// void debug(Args... args)
// {
//   std::ostringstream oss;
//   expand(oss, args...);
//   log(log_level::DEBUG, oss.str());
// }

// template<typename... Args>
// void warn(Args... args)
// {
//   std::ostringstream oss;
//   expand(oss, args...);
//   log(log_level::WARNING, oss.str());
// }


// template<typename... Args>
// void fatal(Args... args)
// {
//   std::ostringstream oss;
//   expand(oss, args...);
//   log(log_level::FATAL, oss.str());
// }

