#include "ziguratipexception.h"
#include "socket.h"
#include "tokenizer.h"
#include "parser.h"
#include "expression.h"
#include "compiler.h"
#include "memory.h"
#include "globals.h"
#include "tcpstream.h"
#include "ipcstream.h"
#include "tcpserver.h"
#include "ipcserver.h"
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

TCPServer tcp_server;
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
    }
  } while (function != "close");

  std::cout << "Transaction Closed " << Globals::memory()->transaction_id() << std::endl;
}

void zigurat_tcp_handler(Socket::handle_t client_handle)
{
  std::unique_ptr<tcpstream> client_stream_deleter(new tcpstream(client_handle, server_blocking_mode, server_timeout));
  Globals::set_client_stream(client_stream_deleter.get());
  handle_client();
}

void zigurat_ipc_handler(Socket::handle_t client_handle)
{
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

  std::cout << "Zigurat is ready ..." << std::endl;

  if (server_type == "TCP") {
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
