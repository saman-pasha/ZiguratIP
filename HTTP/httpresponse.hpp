
#ifndef __HTTPRESPONSE_HPP__
#define __HTTPRESPONSE_HPP__

#include <map>
#include <mutex>
#include <condition_variable>
#include "typebool.hpp"
#include "typeint.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"

namespace Zigurat
{

  class textstream;
  class HTTPRequest;
  
  class HTTPResponse
  {
  public:
    HTTPResponse() = delete;
    HTTPResponse(HTTPRequest*, size_t, volatile size_t&, binarystream&, std::mutex&, std::condition_variable&);
    void set_echo_buffer(textstream*);
    void flush_error(std::string);
    void flush(bool);
    void FLUSH();
    String PROTOCOL();
    void SET_PROTOCOL(String);
    Int STATUS_CODE();
    void SET_STATUS_CODE(Int);
    String REASON_PHRASE();
    void SET_REASON_PHRASE(String);
    String HEADER(String);
    Bool HAS_HEADER(String);
    void set_header(std::string, std::string);
    void SET_HEADER(String, String);
    void REMOVE_HEADER(String);
    String COOKIE(String);
    Bool HAS_COOKIE(String);
    void SET_COOKIE(String, String);
    void REMOVE_COOKIE(String);
    String COOKIE_ATTRIBUTE(String, String);
    Bool HAS_COOKIE_ATTRIBUTE(String, String);
    void SET_COOKIE_ATTRIBUTE(String, String, String = "");
    void REMOVE_COOKIE_ATTRIBUTE(String, String);
    virtual ~HTTPResponse();
  private:
    HTTPRequest* _request;
    size_t _request_id;
    volatile size_t& _dispatch_id;
    binarystream& _stream;
    std::mutex& _stream_access;
    std::condition_variable& _stream_semaphore;
    bool _close;
    std::string _protocol = "HTTP/1.1";
    int _status_code = 200;
    std::string _reason_phrase = "OK";
    std::unique_lock<std::mutex> _lock;
    std::map<std::string, std::string> _headers;
    std::map<std::string, std::map<std::string, std::string> > _cookies;
    std::string _header_case(const std::string&);
    textstream* _echo_buffer = nullptr;
    bool _headers_flushed = false;
  };
	
}

#endif // __HTTPRESPONSE_HPP__
