#include "httpresponse.hpp"
#include "utility.hpp"
#include "httprequest.hpp"
#include "textstream.hpp"
#include <ctime>


namespace Zigurat
{

  // Every line of a response ended with std::endl, which is one \n. HTTP wants
  // CRLF -- RFC 7230 section 3 -- and the blank line that separates the head
  // from the body doubly so. Browsers forgive it, which is why this survived;
  // proxies are entitled not to, and one in front of the server can drop the
  // response or hand back nothing rather than repair it.
  static const char* const CRLF = "\r\n";


  HTTPResponse::HTTPResponse(HTTPRequest* request, size_t request_id, volatile size_t& dispatch_id, 
			     binarystream& stream, std::mutex& stream_access, std::condition_variable& stream_semaphore)
    : _request(request), _request_id(request_id), _dispatch_id(dispatch_id), 
      _stream(stream), _stream_access(stream_access), _stream_semaphore(stream_semaphore), 
      _lock(_stream_access, std::defer_lock)
  {

  }

  String HTTPResponse::PROTOCOL()
  {
    return this->_protocol;
  }

  void HTTPResponse::SET_PROTOCOL(String protocol)
  {
    this->_protocol = protocol.value();
  }

  Int HTTPResponse::STATUS_CODE()
  {
    return this->_status_code;
  }

  void HTTPResponse::SET_STATUS_CODE(Int status_code)
  {
    this->_status_code = status_code.value();
  }

  String HTTPResponse::REASON_PHRASE()
  {
    return this->_reason_phrase;
  }

  void HTTPResponse::SET_REASON_PHRASE(String reason_phrase)
  {
    this->_reason_phrase = reason_phrase.value();
  }

  std::string HTTPResponse::_header_case(const std::string& header)
  {
    std::string result;
    for (size_t i = 0; i < header.size(); i++) {
      if (i == 0) {
        result.push_back(std::toupper(header[i]));
      } else if (header[i - 1] == '-') {
        result.push_back(std::toupper(header[i]));
      } else {
	result.push_back(std::tolower(header[i]));
      }
    }
    return result;
  }

  void HTTPResponse::set_echo_buffer(textstream* echo_buffer)
  {
    this->_echo_buffer = echo_buffer;
  }

  void HTTPResponse::flush_error(std::string msg)
  {
    this->_lock.lock();
    this->_stream_semaphore.wait(this->_lock, [&] () { return (this->_request_id == this->_dispatch_id); });

    // A body, because a refusal with none is a blank page.
    //
    // This carried Content-Length: 0, which is at least honest about there
    // being nothing there -- but a browser shown a zero-length 404 draws
    // nothing at all, and view-source shows nothing either. A page that is
    // missing and a server that is broken then look identical to whoever is
    // trying to use it, which is most of why the Colab trouble took as long as
    // it did to pin down. The other refusals in handle_client already say what
    // happened; this path is the one that did not.
    //
    // Plain text, and only the status: what page was wanted, and why it was
    // refused, are in the log, and neither is a visitor's business.
    const std::string body = msg + "\n";

    std::stringstream ss_response;
    ss_response << "HTTP/1.1 " << msg << CRLF;
    ss_response << "Date: " << Utility::time_to_string(std::time(0), "%a, %d %b %Y %H:%M:%S %Z") << CRLF;
    ss_response << "Server: Zigurat/0.0 (ZiguratIP; " << Utility::os_name() << ")" << CRLF;
    ss_response << "Connection: close" << CRLF;
    ss_response << "Content-Type: text/plain; charset=utf-8" << CRLF;
    ss_response << "Content-Length: " << body.size() << CRLF;
    ss_response << CRLF;
    ss_response << body;
    this->_stream.write(ss_response.str().c_str(), ss_response.tellp());

    this->_dispatch_id++;
    this->_close = true;
    
    this->_lock.unlock();
    this->_stream_semaphore.notify_all();

    this->_headers_flushed = true;
  }

  void HTTPResponse::flush(bool has_content)
  {
    if (this->_headers_flushed) {

      if (this->_echo_buffer->tellp() > 0) {
	this->_stream.write(this->_echo_buffer->str().c_str(), this->_echo_buffer->tellp());
	this->_echo_buffer->str("");
      }

    } else {

      this->_lock.lock();
      this->_stream_semaphore.wait(this->_lock, [&] () { return (this->_request_id == this->_dispatch_id); });

      std::stringstream ss_response;

      ss_response << this->_protocol << " " << this->_status_code << " " << this->_reason_phrase << CRLF;

      for (auto& pair : this->_headers) {
	ss_response << this->_header_case(pair.first) << ": " << pair.second << CRLF;
      }

      if (has_content) {
	ss_response << "Content-Length: " << this->_echo_buffer->tellp() << CRLF;
      }
    
      for (auto& pair : this->_cookies) {
	std::cout << "Set-Cookie" << ": " << pair.first << '=' << pair.second[pair.first] << ';';
	ss_response << "Set-Cookie" << ": " << pair.first << '=' << pair.second[pair.first] << ';';
	for (auto& attr : pair.second) {
	  if (attr.first != pair.first) {
	    std::cout << attr.first;
	    ss_response << attr.first;
	    if (attr.second.size() > 0) {
	      std::cout << '=' << attr.second;
	      ss_response << '=' << attr.second;
	    }
	    std::cout << ';';
	    ss_response << ';';
	  }
	}
	ss_response.seekp(-1, std::ios::cur);
	std::cout << std::endl;
	ss_response << CRLF;
      }
    
      ss_response << CRLF;
      this->_stream.write(ss_response.str().c_str(), ss_response.tellp());
      this->_stream.write(this->_echo_buffer->str().c_str(), this->_echo_buffer->tellp());
      this->_echo_buffer->str("");

      this->_dispatch_id++;
      if (this->_request->_headers.find("CONNECTION") != this->_request->_headers.end() &&
	  this->_request->_headers.at("CONNECTION") == "close") { // Connection: close
	this->_close = true;
      }
      this->_lock.unlock();
      this->_stream_semaphore.notify_all();

      this->_headers_flushed = true;
    }
  }

  void HTTPResponse::FLUSH()
  {
    this->flush(false);
  }

  String HTTPResponse::HEADER(String header)
  {
    header = Utility::to_upper(Utility::trim(header.value()));
    return this->_headers[header.value()];
  }

  Bool HTTPResponse::HAS_HEADER(String header)
  {
    header = Utility::to_upper(Utility::trim(header.value()));
    return (this->_headers.find(header.value()) != this->_headers.end());
  }

  void HTTPResponse::SET_HEADER(String header, String value)
  {
    header = Utility::to_upper(Utility::trim(header.value()));
    this->_headers[header.value()] = value.value();
  }

  void HTTPResponse::set_header(std::string header, std::string value)
  {
    this->_headers[Utility::to_upper(Utility::trim(header))] = value;
  }

  void HTTPResponse::REMOVE_HEADER(String header)
  {
    header = Utility::to_upper(Utility::trim(header.value()));
    this->_headers.erase(header.value());
  }

  String HTTPResponse::COOKIE(String cookie)
  {
    return this->_cookies[cookie.value()][cookie.value()];
  }

  Bool HTTPResponse::HAS_COOKIE(String cookie)
  {
    return (this->_cookies.find(cookie.value()) != this->_cookies.end());
  }

  void HTTPResponse::SET_COOKIE(String cookie, String value)
  {
    this->_cookies[cookie.value()][cookie.value()] = value.value();
  }

  void HTTPResponse::REMOVE_COOKIE(String cookie)
  {
    // erase(end()) is undefined, and removing a cookie that was never set is a
    // perfectly ordinary thing for a page to do.
    this->_cookies.erase(cookie.value());
  }

  String HTTPResponse::COOKIE_ATTRIBUTE(String cookie, String attribute)
  {
    return this->_cookies[cookie.value()][attribute.value()];
  }

  Bool HTTPResponse::HAS_COOKIE_ATTRIBUTE(String cookie, String attribute)
  {
    return (this->_cookies[cookie.value()].find(attribute.value()) != this->_cookies[cookie.value()].end());
  }

  void HTTPResponse::SET_COOKIE_ATTRIBUTE(String cookie, String attribute, String value)
  {
    this->_cookies[cookie.value()][attribute.value()] = value.value();
  }

  void HTTPResponse::REMOVE_COOKIE_ATTRIBUTE(String cookie, String attribute)
  {
    auto iter = this->_cookies.find(cookie.value());
    if (iter != this->_cookies.end()) {
      iter->second.erase(attribute.value());
    }
  }

  HTTPResponse::~HTTPResponse()
  {
    // The request is not owned here. handle_client creates both and the page
    // handler owns both; deleting it here double freed it with the handler's
    // own unique_ptr, which crashed on every single request.
    this->_echo_buffer = nullptr;
    this->_request = nullptr;
  }
	
}
