#include "basepage.hpp"
#include "binarystream.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"


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
