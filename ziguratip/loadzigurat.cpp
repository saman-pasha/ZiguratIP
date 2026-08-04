#include "ziguratipexception.hpp"
#include "socket.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"
#include "expression.hpp"
#include "compiler.hpp"
#include "memory.hpp"
#include "globals.hpp"
#include "tcpstream.hpp"
#include "ipcstream.hpp"
#include "tcpserver.hpp"
#include "tlsserver.hpp"
#include "ipcserver.hpp"
#include "shared.cpp"


using namespace Zigurat;

// binary server configuration
std::string server_type = "TCP";
std::string server_tcp_service = "2160";
#if defined(_WIN32) || defined(_WIN64)
#else
std::string server_ipc_service = "/tmp/zigurat";
#endif
int         server_backlog = 5;
int         server_pool_size = 5;
bool        server_blocking_mode = true;
int         server_timeout = 0;
bool        server_tls_mode = false;

TCPServer tcp_server;
TLSServer tls_server;

extern TLS::HandshakeParameters security_params;
void require_security(const char*);
#if defined(_WIN32) || defined(_WIN64)
#else
IPCServer ipc_server;
#endif


void handle_client()
{
  std::cout << "Transaction Opened " << Globals::memory()->transaction_id() << std::endl;

  Globals::client_stream()->write_std_size(Globals::memory()->transaction_id());  

  std::string function;
  do {

    try {
      Globals::client_stream()->read_std_string(function);
      Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);
    } catch (...) { // Network error
      break;
    }

    try {
      function = Utility::to_lower(function);

      if (function == "echo") {
	std::string text = Globals::client_stream()->read_std_string();
	if (Globals::trace_mode())
	  std::cout << "function 'echo' '" << text << "' " << std::endl;
	Globals::client_stream()->write_std_string(text);

      } else if (function == "auto_commit") {
	bool auto_commit = Globals::client_stream()->read_std_bool();
	if (Globals::trace_mode())
	  std::cout << "function 'auto_commit' '" << auto_commit << "' " << std::endl;
	Globals::memory()->transaction.auto_commit = auto_commit;
	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);

      } else if (function == "isolate") {
	IsolationLevel isolation_level = (IsolationLevel)Globals::client_stream()->read_std_ubyte();
	if (Globals::trace_mode())
	  std::cout << "function 'isolate' '" << (int)isolation_level << "' " << std::endl;
	Globals::memory()->transaction.set_isolation_level(isolation_level);
	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);

      } else if (function == "call") {
	std::string func_name;
	Globals::client_stream()->read_std_string(func_name);
	
	if (Globals::trace_mode())
	  std::cout << "function 'call' '" << func_name << "' " << std::endl;

	func_name = Utility::to_upper(func_name);
#if defined(_WIN32) || defined(_WIN64)
	func_name = library_path + "/_" + func_name + "_.dll";
#else
	func_name = library_path + "/lib_" + func_name + "_.so";
#endif
	auto handle = library_pool.handle(func_name);
	auto symbol = (void (*)(void))library_pool.symbol(handle, "call");

	require_objects(handle);

	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);
	
	symbol();

	if (Globals::memory()->transaction.auto_commit || Globals::default_autocommit_mode())
	  Globals::memory()->commit_transaction();

	if (library_cache_mode != LibraryPool::NONE) library_pool.close(handle);
	
      } else if (function == "compile") {
	if (Globals::trace_mode())
	  std::cout << "function 'compile'" << std::endl;
	std::string code;
	Globals::client_stream()->read_std_text(code);
	if (Globals::trace_mode())
	  std::cout << code << std::endl;
	std::list<Token> tokens;
	Tokenizer::tokenize(code, tokens);
	Expression ast = Globals::parser()->parse("SUITE", tokens);
	Globals::compiler()->compile(ast);
	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);

      } else if (function == "commit") {
	if (Globals::trace_mode())
	  std::cout << "function 'commit'" << std::endl;      
	Globals::memory()->commit_transaction();
	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);

      } else if (function == "rollback") {
	if (Globals::trace_mode())
	  std::cout << "function 'rollback'" << std::endl;      
	Globals::memory()->rollback_transaction();
	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::SUCCESSFUL_DONE);

      } else if (function == "dba_pagefiles") {
	if (Globals::trace_mode())
	  std::cout << "function 'dba_pagefiles'" << std::endl;
	Globals::memory()->dba_pagefiles(*Globals::client_stream());

      } else if (function == "dba_pointers") {
	if (Globals::trace_mode())
	  std::cout << "function 'dba_pointers'" << std::endl;
	Globals::memory()->dba_pointers(*Globals::client_stream());

      } else if (function == "dba_attach_watcher") {
	if (Globals::trace_mode())
	  std::cout << "function 'dba_attach_watcher'" << std::endl;
	Globals::memory()->dba_attach_watcher(Globals::client_stream());

      } else if (function == "dba_detach_watcher") {
	if (Globals::trace_mode())
	  std::cout << "function 'dba_detach_watcher'" << std::endl;
	Globals::memory()->dba_detach_watcher();

      } else if (function.size() == 0) {
	if (Globals::trace_mode())
	  std::cout << "function 'empty'" << std::endl;      
	function = "close";

      } else if (function == "close") {
	if (Globals::trace_mode())
	  std::cout << "function 'close'" << std::endl;      

      } else {
	if (Globals::trace_mode())
	  std::cout << "function error: '" << function << "'" << std::endl;      
	Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::EXCEPTION_THROWN);
	Globals::client_stream()->write_std_string("invalid function '" + function + "'");
	break;
      }
    } catch (ZiguratException& error) {
      if (Globals::trace_mode())
	std::cout << "function error: " << error.message() << std::endl;
      Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::EXCEPTION_THROWN);
      Globals::client_stream()->write_std_string(error.message());
      break;
    } catch (std::exception& error) {
      if (Globals::trace_mode())
	std::cout << "error: " << error.what() << std::endl;
      Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::EXCEPTION_THROWN);
      Globals::client_stream()->write_std_string(error.what());
      break;
    } catch (...) {
      // Nothing a client sends may bring the server down. Anything that gets
      // this far ends the connection, not the process.
      if (Globals::trace_mode())
	std::cout << "error: unknown" << std::endl;
      Globals::client_stream()->write_std_ubyte((uint8_t)ResultType::EXCEPTION_THROWN);
      Globals::client_stream()->write_std_string("unknown error");
      break;
    }
  } while (function != "close");

  // Every branch that reports an error breaks out of the loop, so nothing reads
  // from the socket afterwards to push the buffer out. Unflushed, the error byte
  // died with the connection and the client read a zero off the closed stream --
  // which is SUCCESSFUL_DONE. A failed compile looked like a successful one.
  Globals::client_stream()->flush();

  std::cout << "Transaction Closed " << Globals::memory()->transaction_id() << std::endl;
}

namespace
{
  // The worker threads are pooled, so what a connection binds to its thread has
  // to be unbound again on the way out -- the stream is about to be destroyed,
  // and the next connection served by this thread would otherwise inherit a
  // pointer to it. Whether anything reads it before rebinding is not the point:
  // a pointer to a destroyed stream should not survive the stream, and a call
  // through one lands wherever its vtable used to be. Zeytun has always done
  // this in RequestScope; the binary protocol never did.
  struct ConnectionScope
  {
    ~ConnectionScope()
    {
      Globals::clear_peer();
      Globals::set_client_stream(nullptr);
    }
  };
}

void zigurat_tcp_handler(Socket::handle_t client_handle)
{
  ConnectionScope scope;
  std::unique_ptr<tcpstream> client_stream_deleter(new tcpstream(client_handle, server_blocking_mode, server_timeout));
  Globals::set_client_stream(client_stream_deleter.get());
  handle_client();
}

// The peer has already presented a certificate the authority issued, and is
// registered in the users directory, by the time this is called -- TLSServer
// does not hand over a connection that is neither.
//
// What that certificate grants comes with it. Nothing about the peer is looked
// up, because nothing about the peer is kept: the connection carries its own
// authority, and the same subject on a different certificate is a different set
// of permissions.
void zigurat_tls_handler(tlsstream& client_stream)
{
  ConnectionScope scope;
  Globals::set_client_stream(&client_stream);
  Globals::set_peer(client_stream.peer_subject(), client_stream.peer_permissions());

  std::cout << "Peer '" << Globals::peer_subject() << "' permitted";
  for (const std::string& granted : Globals::peer_permissions())
    std::cout << " '" << granted << "'";
  std::cout << std::endl;

  handle_client();
}

void zigurat_ipc_handler(Socket::handle_t client_handle)
{
  ConnectionScope scope;
  std::unique_ptr<ipcstream> client_stream_deleter(new ipcstream(client_handle, server_blocking_mode, server_timeout));
  Globals::set_client_stream(client_stream_deleter.get());
  handle_client();
}

void load_zigurat(const Configuration& conf)
{
  std::string value;

  conf.get("/SERVER/TYPE", server_type);
  if (server_type == "TCP") {
    if (!conf.get("/SERVER/PORT", server_tcp_service))
      conf.get("/SERVER/SERVICE", server_tcp_service);
    server_tcp_service = Utility::trim(server_tcp_service);
#if defined(_WIN32) || defined(_WIN64)
#else
  } else if (server_type == "IPC") {
    if (!conf.get("/SERVER/PATH", server_ipc_service))
      conf.get("/SERVER/SERVICE", server_ipc_service);
    server_ipc_service = Utility::trim(server_ipc_service);
#endif
  } else {
    throw ZiguratIPException("invalid value for '/SERVER/TYPE'");
  }

  if (conf.get("/SERVER/BACKLOG", value)) {
    std::stringstream spss(value);
    spss >> server_backlog;
  }
	
  if (conf.get("/SERVER/POOL_SIZE", value)) {
    std::stringstream spss(value);
    spss >> server_pool_size;
  }

  if (conf.get("/SERVER/BLOCKING_MODE", value)) {
    value = Utility::to_upper(value);
    if (value == "TRUE")
      server_blocking_mode = true;
    else if (value == "FALSE")
      server_blocking_mode = false;
    else
      throw ZiguratIPException("invalid value for '/SERVER/BLOCKING_MODE'");
  }
	
  if (conf.get("/SERVER/TIMEOUT", value)) {
    std::stringstream spss(value);
    spss >> server_timeout;
  }

  if (conf.get("/SERVER/TLS_MODE", value)) {
    value = Utility::to_upper(Utility::trim(value));
    if (value == "TRUE")
      server_tls_mode = true;
    else if (value == "FALSE")
      server_tls_mode = false;
    else
      throw ZiguratIPException("invalid value for '/SERVER/TLS_MODE'");
  }

  std::cout << "Server type: '" << server_type << "'" << std::endl;
  if (server_type == "TCP") {
    std::cout << "Server service: '" << server_tcp_service << "'" << std::endl;
#if defined(_WIN32) || defined(_WIN64)
#else
  } else if (server_type == "IPC") {
    std::cout << "Server service: '" << server_ipc_service << "'" << std::endl;
#endif
  } else {
    throw ZiguratIPException("invalid value for '/SERVER/TYPE'");
  }
  std::cout << "Server backlog: '" << server_backlog << "'" << std::endl;		
  std::cout << "Server pool size: '" << server_pool_size << "'" << std::endl;		
  std::cout << "Server blocking mode: '" << ((server_blocking_mode) ? "TRUE" : "FALSE" ) << "'" << std::endl;
  std::cout << "Server timeout: '" << server_timeout << "'" << std::endl;
  std::cout << "Server TLS mode: '" << ((server_tls_mode) ? "TRUE" : "FALSE") << "'" << std::endl;

  // Permissions are carried by the certificate a connection presents, so a
  // connection with no certificate has nothing to be judged on and is allowed
  // everything. That is right, and it is also easy to configure by accident:
  // turning permissions on without turning TLS on enforces nothing at all.
  if (Globals::permissions_mode() && !server_tls_mode)
    std::cout << "Zigurat: SECURITY/PERMISSIONS_MODE is on but SERVER/TLS_MODE is not,"
	      << " so nothing is enforced on this port -- a plain connection carries"
	      << " no certificate to judge" << std::endl;

  // Before announcing readiness, so a misconfigured secure server fails to start
  // rather than starting insecure.
  if (server_tls_mode && server_type == "TCP") require_security("Zigurat");
  if (server_tls_mode && server_type != "TCP")
    throw ZiguratIPException("'/SERVER/TLS_MODE' needs '/SERVER/TYPE' to be TCP");

  std::cout << "Zigurat is ready ..." << std::endl;

  if (server_type == "TCP" && server_tls_mode) {
    std::thread server_thread([&] () {
	tls_server.credentials(security_params);
	tls_server.run(TCPServer::IPV4, server_tcp_service, server_backlog, server_pool_size, zigurat_tls_handler);
      });
    server_thread.detach();
  } else if (server_type == "TCP") {
    std::thread server_thread([&] () {
	tcp_server.run(TCPServer::IPV4, server_tcp_service, server_backlog, server_pool_size, zigurat_tcp_handler);
      });
    server_thread.detach();
#if defined(_WIN32) || defined(_WIN64)
#else
  } else if (server_type == "IPC") {
    std::thread server_thread([&] () {		
	ipc_server.run(server_ipc_service, server_backlog, server_pool_size, zigurat_ipc_handler);
      });
    server_thread.detach();		
#endif
  } else {
    throw ZiguratIPException("invalid value for '/SERVER/TYPE'");
  }
}
