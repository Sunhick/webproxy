/****************************************************
 *  Http request header file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <cstring>
#include <string>
#include <vector>
#include <map>
//#include <regex>

namespace webkit {
  class http_request {
  private:
    std::string requestString;
    std::string formattedString;
		
    std::map<std::string, std::string> headers;
    int method;
    std::string relativePath;
    std::string machineName;
    std::string machinePort;
    int version;
		
    int status;
    bool parsed;		
		
    void tokenizeString(std::string &, std::vector<std::string> &, std::string &);
    void setStatus(int);
		
    int setBadRequest();
    int setNotImplemented();
		
    bool setMethod(std::string &);
    bool setVersion(std::string &);
    bool setURI(std::string &);
    void makeFormattedRequest();
		
    void lowerCase(std::string &);
    void upperCase(std::string &);
    void trim(std::string &);
		
  public:
    http_request();		
    http_request(std::string request) ;

    void setRequest(std::string);
		
    int parse();
		
    bool getParsed();
    std::string getFormattedRequest();
    std::string getHostName();
    std::string getStatusString();
    std::string getVersionString();
    std::string getMethodName();
    std::string getFullURL();
    std::string getMachinePort();
		
    void clear();
  };
}

#endif
