#include "httprequest.hpp"
#include "httpexception.hpp"
#include "bufferstream.hpp"
#include "typechar.hpp"
#include "utility.hpp"
#include <cctype>


namespace Zigurat
{

  namespace
  {
    // One hex digit, or -1. Percent-decoding used to hand the two characters to
    // a stringstream and read them back as hex, which cannot say whether it
    // read anything: "%zz" extracted nothing, left the value at zero, and
    // decoded to a NUL that the caller then treated as data.
    int hex_digit(char ch)
    {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
      if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
      return -1;
    }
  }

  HTTPRequest::HTTPRequest(std::string method, std::string uri, std::string protocol, std::map<std::string, std::string> headers, 
			   std::string host, std::string port, char* content, size_t content_length) 
    : _method(method), _uri(uri), _protocol(protocol), _headers(headers), 
      _host(host), _port(port), _content(content), _content_length(content_length)
  {

    this->_load_query_vars(); // $_GET
    
    if (this->_method == "POST" || this->_method == "PUT") {
      if (this->_headers.find("CONTENT-TYPE") != this->_headers.end()) {
	this->_content_type = this->_headers["CONTENT-TYPE"];
	auto parts = Utility::split(this->_content_type, ';');
	this->_content_type = Utility::trim(Utility::to_lower(parts[0]));
	this->_headers["CONTENT-TYPE"] = this->_content_type;
	for (size_t i = 1; i < parts.size(); i++) {
	  auto con_parts = Utility::split(parts[i], '=');
	  this->_headers[Utility::trim(Utility::to_upper(con_parts[0]))] = Utility::trim(parts[1]);
	}
      }
      // Media types are case insensitive (RFC 7231 section 3.1.1.1), and the
      // value is no longer lower-cased on the way in.
      const std::string content_type = Utility::to_lower(this->_content_type);
      if (content_type == "application/x-www-form-urlencoded" || content_type == "multipart/form-data") {
	this->_load_post_vars(); // $_POST
      }
    }

    for (const auto& header : this->_headers) {
      if (header.first == "COOKIE") {
	std::vector<std::string> cookies = Utility::split(header.second, ';');
	for (const auto& cookie : cookies) {
	  std::vector<std::string> name_value = Utility::split(cookie, '=');
	  this->_cookies.insert({Utility::trim(name_value[0]), (name_value.size() > 1) ? name_value[1] : ""});
	}
      }
    } 
  }

  String HTTPRequest::METHOD()
  {
    return this->_method;
  }

  String HTTPRequest::URI()
  {
    return this->_uri;
  }

  String HTTPRequest::PROTOCOL()
  {
    return this->_protocol;
  }

  String HTTPRequest::HOST()
  {
    return this->_host;
  }

  String HTTPRequest::PORT()
  {
    return this->_port;
  }

  String HTTPRequest::PATH()
  {
    return this->_path;
  }

  String HTTPRequest::FRAGMENT()
  {
    return this->_fragment;
  }

  Vector<Char> HTTPRequest::CONTENT()
  {
    return std::vector<char>(this->_content.get(), this->_content.get() + this->_content_length);
  }

  String HTTPRequest::CONTENT_TYPE()
  {
    return this->_content_type;
  }

  ULong  HTTPRequest::CONTENT_LENGTH()
  {
    return (uint64_t)this->_content_length;
  } 

  String HTTPRequest::HEADER(String header)
  {
    header = Utility::to_upper(Utility::trim(header.value()));
    std::map<std::string, std::string>::const_iterator it = this->_headers.find(header.value());
    // Absent reads as empty rather than throwing, matching the response side.
    return (it == this->_headers.end()) ? String(std::string()) : String(it->second);
  }

  Bool HTTPRequest::HAS_HEADER(String header)
  {
    header = Utility::to_upper(Utility::trim(header.value()));
    return (this->_headers.find(header.value()) != this->_headers.end());
  }

  String HTTPRequest::COOKIE(String cookie)
  {
    return this->_cookies[cookie.value()];
  }

  Bool HTTPRequest::HAS_COOKIE(String cookie)
  {
    return (this->_cookies.find(cookie.value()) != this->_cookies.end());
  }

  String HTTPRequest::QUERY(String name)
  {
    name = Utility::to_lower(name.value());
    return this->_query_vars.at(name.value());
  }

  Bool HTTPRequest::HAS_QUERY(String name)
  {
    return (this->_query_vars.find(name.value()) != this->_query_vars.end());
  }

  String HTTPRequest::POST(String name)
  {
    name = Utility::to_lower(Utility::trim(name.value()));
    return this->_post_vars.at(name.value());
  }

  Bool HTTPRequest::HAS_POST(String name)
  {
    return (this->_post_vars.find(name.value()) != this->_post_vars.end());
  }

  Vector<String> HTTPRequest::ARRAY_QUERY(String name)
  {
    name = Utility::to_lower(name.value());
    return this->_query_array_vars.at(name.value());
  }

  Bool HTTPRequest::HAS_ARRAY_QUERY(String name)
  {
    return (this->_query_array_vars.find(name.value()) != this->_query_array_vars.end());
  }

  Vector<String> HTTPRequest::ARRAY_POST(String name)
  {
    name = Utility::to_lower(Utility::trim(name.value()));
    return this->_post_array_vars.at(name.value());
  }

  Bool HTTPRequest::HAS_ARRAY_POST(String name)
  {
    return (this->_post_array_vars.find(name.value()) != this->_post_array_vars.end());
  }

  void HTTPRequest::_vars(std::string name, std::string value, 
			 std::map<std::string, std::string>& vars, 
			 std::map<std::string, std::vector<std::string> >& array_vars)
  {
    name = Utility::to_lower(name);
    if (name.size() > 2 && name[name.size() - 2] == '[' && name[name.size() - 1] == ']') {
      if (array_vars.find(name) == array_vars.end()) {
	std::vector<std::string> _array;
	_array.push_back(value);
	array_vars[name] = _array;
      } else {
	array_vars[name].push_back(value);
      }
    } else {
      vars[name] = value;
    }
  }
  
  void HTTPRequest::_load_query_vars()
  {
    char ch;
    std::string var_name, var_value;
    for (auto iter = this->_uri.begin(); iter < this->_uri.end(); iter++) {
      ch = *iter;
      if (ch == '+') {
	var_value.push_back(' ');
      } else if (ch == '%') {
	// An escape is a % and exactly two hex digits. Both used to be taken
	// with *(++iter) and no test whatever, so "GET /a%" ran the iterator
	// past end() and read whatever lay beyond the string -- reachable in a
	// single request, before any authentication, from anyone who can open
	// the port.
	if (this->_uri.end() - iter < 3) throw HTTPException("400 Bad Request");
	const int hi = hex_digit(*(iter + 1));
	const int lo = hex_digit(*(iter + 2));
	if (hi < 0 || lo < 0) throw HTTPException("400 Bad Request");
	iter += 2;
	var_value.push_back((char)((hi << 4) | lo));
      } else if (ch == '?') {
	this->_path = var_value;
	var_value.clear();
      } else if (ch == '=') {
	var_name = var_value;
	var_value.clear();
      } else if (ch == '&') {
	this->_vars(var_name, var_value, this->_query_vars, this->_query_array_vars);
	var_name.clear();
	var_value.clear();
      } else if (ch == '#') {
	if (this->_path.size() == 0) {
	  this->_path = var_value;
	  var_value.clear();
	} else {
	  this->_vars(var_name, var_value, this->_query_vars, this->_query_array_vars);
	  var_name.clear();
	  var_value.clear();
	}
	// push_back(ch) here, and ch is the '#' that got us in -- so a fragment
	// came out as a run of hashes the right length rather than its own text.
	while (++iter != this->_uri.end()) {
	  var_value.push_back(*iter);
	}
	this->_fragment = var_value;
	var_value.clear();
	break;
      } else {
	var_value.push_back(ch);
      }
    }
    if (this->_path.size() == 0) {
      this->_path = var_value;
      var_name.clear();
      var_value.clear();
    } else if (var_name.size() > 0) {
      this->_vars(var_name, var_value, this->_query_vars, this->_query_array_vars);
      var_name.clear();
      var_value.clear();
    }
  }

  void HTTPRequest::_load_post_vars()
  {
    // The body is not a C string. httpserver.cpp allocates new char[length] and
    // reads exactly that many octets into it, so there is no terminator; the
    // bufferstream constructor takes a std::string by value, so handing it the
    // bare pointer ran strlen() off the end of the allocation and kept going
    // until it happened on a zero byte -- on every POST, and whatever it swept
    // up on the way became form variables. Give it the length we were told.
    if (!this->_content || this->_content_length == 0) return;

    uint8_t ch;
    bufferstream buffer(std::string(this->_content.get(), this->_content_length));
    std::string var_name, var_value;
    while (!buffer.eof()) {
      buffer.read_std_ubyte(ch);
      if (ch == '+') {
	var_value.push_back(' ');
      } else if (ch == '%') {
	// Same escape, same rule as the query string. The two reads were not
	// checked either, so a body ending in '%' left both characters holding
	// whatever the stack had in them and decoded that.
	uint8_t ch1 = 0, ch2 = 0;
	buffer.read_std_ubyte(ch1);
	buffer.read_std_ubyte(ch2);
	if (buffer.eof() || buffer.fail()) throw HTTPException("400 Bad Request");
	const int hi = hex_digit((char)ch1);
	const int lo = hex_digit((char)ch2);
	if (hi < 0 || lo < 0) throw HTTPException("400 Bad Request");
	var_value.push_back((uint8_t)((hi << 4) | lo));
      } else if (ch == '=') {
	var_name = var_value;
	var_value.clear();
      } else if (ch == '&') {
	this->_vars(var_name, var_value, this->_post_vars, this->_post_array_vars);
	var_name.clear();
	var_value.clear();
      } else {
	var_value.push_back(ch);
      }
    }
    if (var_name.size() > 0) {
      this->_vars(var_name, var_value, this->_post_vars, this->_post_array_vars);
      var_name.clear();
      var_value.clear();
    }
    buffer.clear();
  }

  HTTPRequest::~HTTPRequest()
  {

  }
	
}
