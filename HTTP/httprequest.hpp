
#ifndef __HTTPREQUEST_HPP__
#define __HTTPREQUEST_HPP__

#include <vector>
#include <memory>
#include <map>
#include "typebool.hpp"
#include "typestring.hpp"
#include "typevector.hpp"

namespace Zigurat
{

  class HTTPResponse;

  class HTTPRequest
  {
  public:
    HTTPRequest() = delete;
    HTTPRequest(std::string, std::string, std::string, std::map<std::string, std::string>, std::string, std::string, char*, size_t);
    
  private:
    const std::string _method;
    const std::string _uri;
    const std::string _protocol;
    std::map<std::string, std::string> _headers;
    const std::string _host;
    const std::string _port;
    const std::unique_ptr<char[]> _content;
    const size_t _content_length;
    std::string _path;
    std::string _fragment;
    std::string _content_type;
    std::map<std::string, std::string> _cookies;
    std::map<std::string, std::string> _query_vars;
    std::map<std::string, std::string> _post_vars;
    std::map<std::string, std::vector<std::string> > _query_array_vars;
    std::map<std::string, std::vector<std::string> > _post_array_vars;
    void _vars(std::string, std::string, std::map<std::string, std::string>&, std::map<std::string, std::vector<std::string> >&);
    void _load_query_vars();
    void _load_post_vars();

  public:
    String METHOD();
    String URI();
    String PROTOCOL();
    String HOST();
    String PORT();
    String PATH();
    String FRAGMENT();
    Vector<Char> CONTENT();
    String CONTENT_TYPE();
    ULong  CONTENT_LENGTH();
    String HEADER(String);
    Bool HAS_HEADER(String);
    String COOKIE(String);
    Bool HAS_COOKIE(String);
    String QUERY(String);
    Bool HAS_QUERY(String);
    String POST(String);
    Bool HAS_POST(String);
    Vector<String> ARRAY_QUERY(String);
    Bool HAS_ARRAY_QUERY(String);
    Vector<String> ARRAY_POST(String);
    Bool HAS_ARRAY_POST(String);
    virtual ~HTTPRequest();
    friend class HTTPResponse;
  };
	
}

#endif // __HTTPREQUEST_HPP__
