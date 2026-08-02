#include "httprequest.hpp"
#include "bufferstream.hpp"
#include "typechar.hpp"
#include "utility.hpp"


namespace Zigurat
{

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
      if (this->_content_type == "application/x-www-form-urlencoded" || this->_content_type == "multipart/form-data") {
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
	char ch1, ch2;
	ch1 = *(++iter);
	ch2 = *(++iter);
	std::string str_hex;
	str_hex.push_back(ch1);
	str_hex.push_back(ch2);
	std::stringstream ss_hex(str_hex);
	uint16_t int_hex;
	ss_hex >> std::hex >> int_hex;
	var_value.push_back((char)int_hex);
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
	while (++iter != this->_uri.end()) {
	  var_value.push_back(ch);
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
    uint8_t ch;
    bufferstream buffer((char*)this->_content.get());
    std::string var_name, var_value;
    while (!buffer.eof()) {
      buffer.read_std_ubyte(ch);
      if (ch == '+') {
	var_value.push_back(' ');
      } else if (ch == '%') {
	uint8_t ch1, ch2;
	buffer.read_std_ubyte(ch1);
	buffer.read_std_ubyte(ch2);
	std::string str_hex;
	str_hex.push_back(ch1);
	str_hex.push_back(ch2);
	std::stringstream ss_hex(str_hex);
	uint16_t int_hex;
	ss_hex >> std::hex >> int_hex;
	var_value.push_back((uint8_t)int_hex);
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
