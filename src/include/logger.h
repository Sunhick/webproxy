/****************************************************
 *  logger header file:
 *  logger is thread safe
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include <map>
#include <mutex>
#include <string>
#include <memory>
#include <iostream>

namespace diagnostics {

  enum color {
    FG_RED      = 31,
    FG_GREEN    = 32,
    FG_BLUE     = 34,
    FG_CYAN     = 36,
    FG_DEFAULT  = 39,
    BG_RED      = 41,
    BG_GREEN    = 42,
    BG_BLUE     = 44,
    BG_DEFAULT  = 49
  };

  class modifier {
    color code;
  public:
    modifier(color code) : code(code) { }
    friend std::ostream& operator<<(std::ostream& os, const modifier& mod) {
      return os << "\033[" << mod.code << "m";
    }
  };

  enum log_level {
    DEFAULT,
    INFO,
    DEBUG,
    WARNING,
    FATAL
  };

  class logger {
  private:
    std::mutex writer;
    std::map<log_level, modifier> colors;
    void log(log_level level, std::string msg);

  public:
    logger();
    ~logger();

    static std::shared_ptr<logger> instance;
    static std::shared_ptr<logger> get_logger();

    void info(std::string msg);
    void debug(std::string msg);
    void warn(std::string msg);
    void fatal(std::string msg);
  };
}

#endif
