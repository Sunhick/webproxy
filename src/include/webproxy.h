/****************************************************
 *  web proxy header file
 *
 * Author : Sunil bn <sunhick@gmail.com>
 *****************************************************/
#ifndef WEB_PROXY_H
#define WEB_PROXY_H

namespace webkit {
  class abstract_proxy {
  public:
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool abort() = 0;
  };
  
  class web_proxy : public abstract_proxy {
  public:
    bool start() override;
    bool stop() override;
    bool abort() override;
  };
}

#endif
