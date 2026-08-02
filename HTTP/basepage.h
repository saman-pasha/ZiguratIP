
#ifndef __BASEPAGE_H__
#define __BASEPAGE_H__

namespace Zigurat
{

  class binarystream;
  class HTTPRequest;
  class HTTPResponse;

  class BasePage
  {
  protected:
    binarystream& _client;
    HTTPRequest& REQUEST; 
    HTTPResponse& RESPONSE;

  public:
    BasePage(binarystream&, HTTPRequest&, HTTPResponse&);
    virtual void PAGE_LOAD();
    virtual ~BasePage();
  };

}

#endif // __BASEPAGE_H__
