#include "basepage.h"
#include "binarystream.h"
#include "httprequest.h"
#include "httpresponse.h"


namespace Zigurat
{

  BasePage::BasePage(binarystream& client, HTTPRequest& request, HTTPResponse& response)
    : _client(client), REQUEST(request), RESPONSE(response)
  {

  }

  void BasePage::PAGE_LOAD()
  {

  }

  // Pages are deleted through a BasePage* by the generated delete_page.
  BasePage::~BasePage()
  {

  }

}
