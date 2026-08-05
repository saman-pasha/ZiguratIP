#include "httpserver.hpp"
#include "httpexception.hpp"
#include "utility.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstring>
#include <condition_variable>
#include "httprequest.hpp"
#include "httpresponse.hpp"
#include "bufferstream.hpp"
#include "tcpstream.hpp"
#include "thread.hpp"


namespace Zigurat
{

  HTTPServer::HTTPServer()
    : _server()
  {

  }
  
  void HTTPServer::handle_client(binarystream& stream, client_handler_t handler, bool blocking_mode, int timeout, 
				 bool async_mode, size_t max_uri_length, size_t max_headers_length, size_t max_content_length)
  {
    std::string line, method, uri, protocol;
    std::map<std::string, std::string> headers;
    std::string host, port;
    char *content = nullptr;
    size_t content_length = 0;   // read for GET too, so it cannot start as garbage
    size_t headers_length = 0;   // octets of head read for the request in hand
    std::deque<Thread> pipes;
    size_t request_id = 1;
    volatile size_t dispatch_id = 1;
    std::mutex stream_access;
    std::condition_variable stream_semaphore;

    // Every refusal below -- 400, 411, 413, 414, 501 -- used to leave this
    // function by unwinding past its caller, which put not one byte on the
    // connection before dropping it. The client sees a socket that closed
    // without answering: a browser draws a blank page, and a proxy in front of
    // one reports the server as broken. Neither can say what was wrong,
    // because nothing was ever said. HEAD is the common way to trip it -- the
    // request line accepts it and the body of the loop does not implement it,
    // so a proxy's HEAD probe got silence rather than the 501 it was owed.
    try {

    while (!stream.eof()) {

      std::getline(stream, line);

      // A keep-alive connection the client has closed reads as an empty line
      // on a stream that is no longer good. That is the end of the
      // conversation, not a malformed request -- treating it as one rejected
      // every completed exchange with a 400.
      if (line.empty() && !stream.good()) break;

      // HTTP lines end CRLF but getline only consumes the LF. Without dropping
      // the CR the blank separator line reads as "\r", so the end of the
      // headers is never recognised and every request fails to parse.
      if (line.size() > 0 && line[line.size() - 1] == '\r') line.erase(line.size() - 1);

      // MAX_HEADERS_LENGTH was configured, documented, and passed down through
      // five signatures without ever being compared to anything. A client
      // could therefore send header lines until the process ran out of memory:
      // each one is kept in the map, and nothing was counting. Count the head
      // as it arrives, including the CRLF that getline consumed, and refuse
      // once it is longer than the configured limit.
      headers_length += line.size() + 2;
      if (max_headers_length > 0 && headers_length > max_headers_length)
	throw HTTPException("431 Request Header Fields Too Large");

      if (line.size() == 0) { // End of headers

	if (headers.find("HOST") == headers.end()) {
	  throw HTTPException("400 Bad Request");
	} else {
	  auto parts = Utility::split(headers["HOST"], ':');
	  host = parts[0];
	  if (parts.size() == 2) port = parts[1];

	  if (method == "GET") {
	    // No payload
	  } else if (method == "POST") {
	    if (headers.find("CONTENT-LENGTH") == headers.end()) {
	      throw HTTPException("411 Length Required");
	    } else {
	      const std::string& declared = headers["CONTENT-LENGTH"];

	      // strtoull answers 0 for anything it cannot read, so a header of
	      // "Content-Length: banana" used to mean a body of nothing rather
	      // than a bad request.
	      if (declared.empty() || declared.find_first_not_of("0123456789") != std::string::npos)
		throw HTTPException("400 Bad Request");

	      content_length = std::strtoull(declared.c_str(), NULL, 10);
	      if (content_length > max_content_length) throw HTTPException("413 Payload Too Large");

	      // Held here rather than as a bare pointer: the read below can fail,
	      // and it used to throw straight past the only thing that knew about
	      // this allocation. HTTPRequest takes ownership at the bottom, so a
	      // client that announced a large body and then went away leaked all
	      // of it, every time, which is a request anyone can repeat.
	      std::unique_ptr<char[]> payload(new char[content_length]);
	      if ((size_t)stream.read_exact(payload.get(), content_length) < content_length)
		throw HTTPException("400 Bad Request");
	      content = payload.release();   // HTTPRequest's unique_ptr owns it now
	    }
	  } else {
	    throw HTTPException("501 Not Implemented");
	  }

	  HTTPRequest* request = new HTTPRequest(method, uri, protocol, headers, host, port, content, content_length);
	  HTTPResponse* response = new HTTPResponse(request, request_id++, dispatch_id, stream, stream_access, stream_semaphore);

	  response->set_header("Date", Utility::time_to_string(std::time(0), "%a, %d %b %Y %H:%M:%S %Z"));
	  response->set_header("Server", "Zeytun/0.0 (ZiguratIP; " + Utility::os_name() + ")");
	  if (blocking_mode) {
	    response->set_header("Connection", "keep-alive");
	    // "timeout=N", with an equals sign. RFC 7230's parameter grammar --
	    // and every proxy's parser -- reads name=value; "timeout:60" is not a
	    // parameter at all, so a proxy pooling this connection had nothing to
	    // go on and fell back to a guess of its own.
	    response->set_header("Keep-Alive", "timeout=" + std::to_string(timeout));
	  } else {
	    response->set_header("Connection", "close");
	  }

	  // RESET FOR NEW PIPE
	  //
	  // The request line is only read when method is empty -- that is how the
	  // loop tells it from a header line -- so leaving method set meant the
	  // next request's "GET /page HTTP/1.1" was taken for a header, found to
	  // have no colon in it, and answered 400. Every connection therefore
	  // served exactly one request, however loudly the response advertised
	  // keep-alive. Browsers hid it by opening a fresh connection each time; a
	  // proxy that pools them saw a 502 on every reuse.
	  method.clear();
	  uri.clear();
	  protocol.clear();
	  host.clear();
	  port.clear();
	  content_length = 0;
	  headers_length = 0;   // the next request on this connection starts fresh

	  headers.clear();
	  content = nullptr;

	  // PAGE LOADS
	  if (async_mode) {
	    pipes.emplace_back(handler, &stream, request, response);
	  } else {
	    handler(&stream, request, response);
	  }
		      
	}

      } else if (method.size() == 0) { // First line of head

	auto parts = Utility::split(line, ' ');
	if (parts.size() == 3) {
	  method = Utility::trim(parts[0]);
	  uri = Utility::trim(parts[1]);
	  protocol = Utility::trim(parts[2]);
	  if (!(method == "GET" || method == "HEAD" || method == "POST" || method == "PUT" || method == "DELETE")) {
	    throw HTTPException("501 Not Implemented");
	  } else if (method.size() == 0 || uri.size() == 0 || protocol.size() == 0) {
	    throw HTTPException("400 Bad Request");
	  } else  if (uri.size() > max_uri_length) {
	    throw HTTPException("414 URI Too Long");
	  }
	} else {
	  throw HTTPException("400 Bad Request");
	}

      } else { // Each line of head

	auto parts = Utility::split(line, ':');
	if (parts.size() > 1) {
	  std::string field_name = Utility::to_upper(parts[0]);
	  if (field_name[field_name.size() - 1] == ' ' || field_name[field_name.size() - 1] == '\t')
	    throw HTTPException("400 Bad Request");
	  std::string field_value;
	  if (parts.size() == 2) {
	    field_value = Utility::to_lower(Utility::trim(parts[1]));
	  } else {
	    std::stringstream ss_value;
	    ss_value << Utility::to_lower(Utility::trim(parts[1]));
	    for (size_t i = 2; i < parts.size(); i++)
	      ss_value << ':' << Utility::to_lower(Utility::trim(parts[i]));
	    field_value = ss_value.str();
	  }
	  if (headers.find(field_name) == headers.end()) { // Single header
	    headers[field_name] = field_value;
	  } else { // Combine header
	    if (field_name == "HOST" || field_name == "CONTENT_LENGTH")
	      throw HTTPException("400 Bad Request");
	    headers[field_name] = headers[field_name] + "," + field_value;
	  }
	} else {
	  throw HTTPException("400 Bad Request");
	}

      }

      // Wait for all pipes to be done
      if (async_mode) {
	for (Thread& pipe : pipes)
	  pipe.join();
	pipes.clear();
      }

    } // EOF

    } catch (const HTTPException& ex) {
      // Anything still running has to finish before the socket is written to,
      // or its output interleaves with the error.
      if (async_mode) {
	for (Thread& pipe : pipes)
	  pipe.join();
	pipes.clear();
      }

      const std::string status = ex.what();          // "501 Not Implemented"
      const std::string body = status + "\n";
      std::stringstream out;
      out << "HTTP/1.1 " << status << "\r\n"
	  << "Content-Type: text/plain; charset=utf-8\r\n"
	  << "Content-Length: " << body.size() << "\r\n"
	  << "Connection: close\r\n"
	  << "\r\n"
	  << body;

      // Best effort: the peer that sent a malformed request is often the same
      // peer that has already gone away, and failing to report a failure must
      // not become a second one.
      try {
	stream << out.str();
	stream.flush();
      } catch (...) {
      }
    }
  }

  void HTTPServer::run(TCPServer::Version version, std::string service, int backlog, size_t pool_size, 
		       client_handler_t handler, bool blocking_mode, int timeout,
		       bool async_mode, size_t max_uri_length, size_t max_headers_length, size_t max_data_length)
  {
    this->_server.run(version, service, backlog, pool_size, [=] (Socket::handle_t client_handle) {
	tcpstream stream(client_handle, blocking_mode, timeout);
	HTTPServer::handle_client(stream, handler, blocking_mode, timeout, async_mode, max_uri_length, max_headers_length, max_data_length);
      });
  }

  void HTTPServer::run(const TLS::HandshakeParameters& params,
		       TCPServer::Version version, std::string service, int backlog, size_t pool_size,
		       client_handler_t handler, bool blocking_mode, int timeout,
		       bool async_mode, size_t max_uri_length, size_t max_headers_length, size_t max_data_length)
  {
    this->_server.credentials(params);
    this->_server.run(version, service, backlog, pool_size, [=] (tlsstream& stream) {
	HTTPServer::handle_client(stream, handler, blocking_mode, timeout, async_mode,
				  max_uri_length, max_headers_length, max_data_length);
      });
  }

  void HTTPServer::shutdown(bool force)
  {
    this->_server.shutdown(force);
  }

  HTTPServer::~HTTPServer()
  {
    this->_server.shutdown(true);
  }

}
