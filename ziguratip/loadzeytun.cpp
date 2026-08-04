#include "ziguratipexception.hpp"
#include "httpexception.hpp"
#include "filestream.hpp"
#include "textstream.hpp"
#include "basepage.hpp"
#include "memory.hpp"
#include "globals.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"
#include "configuration.hpp"
#include "httpserver.hpp"
#include "tlsserver.hpp"
#include "session.hpp"
#include "shared.cpp"


using namespace Zigurat;

// HTTP server configuration
std::string http_service = "80";
int         http_backlog = 5;
int         http_pool_size = 5;
bool        http_blocking_mode = true;
size_t      http_timeout = 0;
bool        http_async_mode = true;
size_t      max_uri_length = 8000;
size_t      max_headers_length = 16000;
size_t      max_content_length = 2000000000;
std::time_t session_timeout = 1800;   // seconds a session may sit idle

// HTTP mime types configuration
std::string   mime_path;
Configuration mime_types;

HTTPServer http_server;
bool       http_tls_mode = false;

extern TLS::HandshakeParameters security_params;
void require_security(const char*);

typedef BasePage* (*NEW_PAGE)(binarystream*, HTTPRequest&, HTTPResponse&);
typedef void (*DELETE_PAGE)(BasePage*);


namespace
{
  // One HTTP request is one transaction. The worker threads are pooled, so
  // everything a request binds to its thread -- the transaction, the session,
  // the client and echo streams -- has to be unbound again on the way out, or
  // the next visitor served by that thread inherits it.
  class RequestScope
  {
  private:
    bool _committed = false;

  public:
    RequestScope()
    {
      if (Globals::memory()) Globals::memory()->begin_transaction();
    }

    void commit()
    {
      if (Globals::memory() && !this->_committed) {
	Globals::memory()->commit_transaction();
	this->_committed = true;
      }
    }

    ~RequestScope()
    {
      try {
	// Anything the page left uncommitted is discarded: a request that did
	// not finish cleanly must not leave half its writes behind.
	if (Globals::memory() && !this->_committed) Globals::memory()->rollback_transaction();
      } catch (...) {
      }

      Session::RELEASE();
      Globals::clear_peer();
      Globals::set_echo_stream(nullptr);
      Globals::set_client_stream(nullptr);
    }
  };
}

void zeytun_handler (binarystream* client, HTTPRequest* request, HTTPResponse* response) {
  std::cout << "Response Starts " << std::this_thread::get_id() << std::endl;

  std::unique_ptr<HTTPRequest> request_deleter(request);
  std::unique_ptr<HTTPResponse> response_deleter(response);

  Globals::set_client_stream(client);

  // Over a secure connection the visitor is whoever their certificate says, on
  // every request, with no cookie and no login: HTTP is stateless and so is
  // this. The worker threads are pooled, so RequestScope unbinds it again on
  // the way out. A plain connection has no peer, and nothing is enforced.
  if (tlsstream* secure = dynamic_cast<tlsstream*>(client))
    Globals::set_peer(secure->peer_subject(), secure->peer_permissions());

  std::unique_ptr<textstream> echo_deleter(new textstream);

  response->set_echo_buffer(echo_deleter.get());
  Globals::set_echo_stream(echo_deleter.get());

  // Sweep idle sessions before serving, so the store cannot grow without bound.
  Session::EXPIRE(session_timeout);

  RequestScope scope;

  try {
    std::string path = Utility::to_lower(request->PATH().value());
    if (path == "/") {
      std::string index_path = home_path + "http/index.html";
      std::ifstream index_html(index_path);
      if (index_html.good()) {
	path = "/index.html";
      } else {
	path = "/index.zt";
      }
      index_html.close();
    }

    if (path.find("..") != std::string::npos) {
      throw HTTPException("404 Not Found");      
    }

    std::string name = path;
    std::string ext = "";
    size_t index = path.find_last_of('.');
    if (index > 0) {
      name = path.substr(0, index);
      ext = path.substr(index + 1, path.size());
    }

    if (ext == "zt") {
      auto parts = Utility::split(name, '/');
      name.clear();
      for (size_t i = 1; i < parts.size(); i++) {
	if (i == 1)
	  name += Utility::to_upper(parts[i]);	  
	else
	  name += "::" + Utility::to_upper(parts[i]);
      }

#if defined(_WIN32) || defined(_WIN64)
      std::string page_path = library_path + "/_" + name + "_.dll";
#else
      std::string page_path = library_path + "/lib_" + name + "_.so";
#endif

      // LibraryLoader::handle throws when dlopen fails rather than returning
      // null, so the check below it was dead: a request for a page that does not
      // exist fell through to the catch-all, which answered 200 with the dlopen
      // message -- absolute server paths and all -- in the body. A page that is
      // not there is a 404, and the client is told nothing about the filesystem.
      LibraryLoader::handle_t handle = nullptr;
      try {
	handle = library_pool.handle(page_path);
      } catch (const std::exception& why) {
	std::cout << "Zeytun: no page '" << name << "': " << why.what() << std::endl;
	throw HTTPException("404 Not Found");
      }

      if (!handle)
	throw HTTPException("404 Not Found");

      // Not a refusal and not the visitor's fault, so it is not a 403: the
      // server is the thing that is wrong, and the answer says so.
      require_runtime(handle);

      // A page is not something a certificate can name -- Zeytun decides which
      // URLs it serves -- so what is checked is what the page reaches: the
      // tables and procedures it requires.
      //
      // Over HTTP that is a 403, not a server error: the request was
      // understood and refused. Which object was refused goes to the log, not
      // to the visitor, who has no business learning what the page touches.
      try {
	require_objects(handle);
      } catch (const ZiguratException& refused) {
	std::cout << "Zeytun: refusing '" << name << "': " << refused.message() << std::endl;
	if (library_cache_mode != LibraryPool::NONE) library_pool.close(handle);
	throw HTTPException("403 Forbidden");
      }

      NEW_PAGE new_sym = (NEW_PAGE)library_pool.symbol(handle, "new_page");
      DELETE_PAGE del_sym = (DELETE_PAGE)library_pool.symbol(handle, "delete_page");

      if (!new_sym || !del_sym)
	throw HTTPException("404 Not Found");

      auto page = new_sym(client, *request, *response);
      page->PAGE_LOAD();
      del_sym(page);

      if (library_cache_mode != LibraryPool::NONE) library_pool.close(handle);

    } else {

      std::string content_type = "application/" + ext;
      mime_types.get("/" + ext + "/", content_type);
      response->SET_HEADER("Content-Type", content_type);
      std::string file_path = home_path + "http" + path;
      std::ifstream file(file_path, std::ios::binary | std::ios::ate);
      if (file.good()) {
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
        char buffer[size];
	if (file.read(buffer, size)) {
	  Globals::echo_stream()->write(buffer, size);
	}
      } else {
	throw HTTPException("404 Not Found");
      }
      file.close();
    }

    scope.commit();
    response->flush(true);

    std::cout << "Response Ends " << std::this_thread::get_id()
	      << " (transaction committed)" << std::endl << std::endl;

  } catch (HTTPException& ex) {
    std::cout << "Zeytun Catch: " << ex.what() << std::endl;
    response->flush_error(ex.what());

  } catch (ZiguratException& ex) {
    std::cout << "Zeytun Catch: " << ex.what() << std::endl;
    response->SET_HEADER("Content-Type", "text/html");
    *Globals::echo_stream() << "<html><title>Error</title><body>Server side error code: " + 
      std::to_string(ex.code()) + ", message: " + ex.message() + "</body></html>";
    response->flush(true);

  } catch (std::exception& ex) {
    // What went wrong goes to the log, not to the client. The message from an
    // arbitrary failure carries whatever the failing code had to hand -- dlopen
    // reports the full path it tried -- and none of that is the caller's
    // business.
    std::cout << "Zeytun Catch: " << ex.what() << std::endl;
    response->flush_error("500 Internal Server Error");
  }
}

void load_zeytun(const Configuration& conf)
{
  std::string value = "";

  if (conf.get("/HTTP/PORT", value) || conf.get("/HTTP/SERVICE", value))
    http_service = Utility::trim(value);
  std::cout << "HTTP service: '" << http_service << "'" << std::endl;

  if (conf.get("/HTTP/SESSION_TIMEOUT", value)) {
    std::stringstream spss(value);
    spss >> session_timeout;
  }
  std::cout << "HTTP session timeout: '" << session_timeout << "'" << std::endl;

  if (conf.get("/HTTP/POOL_SIZE", value)) {
    std::stringstream spss(value);
    spss >> http_pool_size;
  }
  std::cout << "HTTP pool size: '" << http_pool_size << "'" << std::endl;

  if (conf.get("/HTTP/BACKLOG", value)) {
    std::stringstream spss(value);
    spss >> http_backlog;
  }
  std::cout << "HTTP backlog: '" << http_backlog << "'" << std::endl;

  if (conf.get("/HTTP/BLOCKING_MODE", value)) {
    value = Utility::to_upper(value);
    if (value == "TRUE")
      http_blocking_mode = true;
    else if (value == "FALSE")
      http_blocking_mode = false;
    else
      throw ZiguratIPException("invalid value for '/HTTP/BLOCKING_MODE'");
  }
  std::cout << "HTTP blocking mode: '" << ((http_blocking_mode) ? "TRUE" : "FALSE" ) << "'" << std::endl;

  if (conf.get("/HTTP/TIMEOUT", value)) {
    std::stringstream spss(value);
    spss >> http_timeout;
  }
  std::cout << "HTTP timeout: '" << http_timeout << "'" << std::endl;

  if (conf.get("/HTTP/ASYNCHRONOUS_MODE", value)) {
    value = Utility::to_upper(value);
    if (value == "TRUE")
      http_async_mode = true;
    else if (value == "FALSE")
      http_async_mode = false;
    else
      throw ZiguratIPException("invalid value for '/HTTP/ASYNCHRONOUS_MODE'");
  }
  std::cout << "HTTP asynchronous mode: '" << ((http_async_mode) ? "TRUE" : "FALSE" ) << "'" << std::endl;

  if (conf.get("/HTTP/MAX_URL_LENGTH", value) || conf.get("/HTTP/MAX_URI_LENGTH", value)) {
    std::stringstream spss(value);
    spss >> max_uri_length;
  }
  std::cout << "Max uri length: '" << max_uri_length << "'" << std::endl;

  if (conf.get("/HTTP/MAX_HEADERS_LENGTH", value)) {
    std::stringstream spss(value);
    spss >> max_headers_length;
  }
  std::cout << "Max headers length: '" << max_headers_length << "'" << std::endl;

  if (conf.get("/HTTP/MAX_CONTENT_LENGTH", value)) {
    std::stringstream spss(value);
    spss >> max_content_length;
  }
  std::cout << "Max content length: '" << max_content_length << "'" << std::endl;

  if (conf.get("/HTTP/TLS_MODE", value)) {
    value = Utility::to_upper(Utility::trim(value));
    if (value == "TRUE")
      http_tls_mode = true;
    else if (value == "FALSE")
      http_tls_mode = false;
    else
      throw ZiguratIPException("invalid value for '/HTTP/TLS_MODE'");
  }
  std::cout << "Zeytun TLS mode: '" << ((http_tls_mode) ? "TRUE" : "FALSE") << "'" << std::endl;

  // As on the binary protocol: a visitor with no certificate has nothing to be
  // judged on, so permissions without TLS enforce nothing here either. Worth
  // saying, because a reverse proxy in front makes this the ordinary setup.
  if (Globals::permissions_mode() && !http_tls_mode)
    std::cout << "Zeytun: SECURITY/PERMISSIONS_MODE is on but HTTP/TLS_MODE is not,"
	      << " so every page is served to anybody who asks" << std::endl;

  // Before announcing readiness, so a misconfigured secure server fails to start
  // rather than starting insecure.
  if (http_tls_mode) require_security("Zeytun");

  mime_path = Utility::config_path("mime.conf");
  conf.get("/HTTP/MIME_FILE", mime_path);
  mime_types.load(mime_path);
  std::cout << "Mime-types file: '" << mime_path << "'" << std::endl;
	
  std::cout << "Zeytun is ready ..." << std::endl << std::endl;
	
  if (http_tls_mode) {
    http_server.run(security_params, TCPServer::IPV4, http_service, http_backlog, http_pool_size,
		    zeytun_handler, http_blocking_mode, http_timeout,
		    http_async_mode, max_uri_length, max_headers_length, max_content_length);
  } else {
    http_server.run(TCPServer::IPV4, http_service, http_backlog, http_pool_size,
		    zeytun_handler, http_blocking_mode, http_timeout,
		    http_async_mode, max_uri_length, max_headers_length, max_content_length);
  }
}
