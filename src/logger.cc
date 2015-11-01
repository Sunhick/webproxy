/****************************************************
 *  logger implementation file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#include "include/logger.h"

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
}

logger::~logger() 
{

}

std::shared_ptr<logger> logger::get_logger()
{
  if (!instance) {
    instance = std::make_shared<logger>();
  }

  return instance;
}

void logger::log(log_level level, std::string msg) 
{
  auto enumstr = [](log_level ll) {
    switch(ll) {
    case log_level::DEBUG: return "DEBUG::";
    case log_level::INFO: return "INFO::";
    case log_level::FATAL: return "FATAL::";
    case log_level::WARNING: return "WARN::";
    default: return "DEFAULT::";
    }  
  };
  std::string type = enumstr(level);

  std::lock_guard<std::mutex> lock(writer);
  std::cout << colors.at(level) << type << msg << colors.at(log_level::DEFAULT) << std::endl;
}

void logger::info(std::string msg)
{
  log(log_level::INFO, msg);
}

void logger::debug(std::string msg)
{
  log(log_level::DEBUG, msg);
}

void logger::warn(std::string msg)
{
  log(log_level::WARNING, msg);
}

void logger::fatal(std::string msg)
{
  log(log_level::FATAL, msg);
}

