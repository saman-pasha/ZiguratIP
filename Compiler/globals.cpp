#include "globals.hpp"
#include "utility.hpp"


bool Globals::_reset_mode = false;
bool Globals::_trace_mode = false;
bool Globals::_permissions_mode = false;
bool Globals::_default_autocommit_mode = false;
Zigurat::IsolationLevel Globals::_default_isolation_level = Zigurat::IsolationLevel::READ_COMMITTED;

thread_local Zigurat::binarystream* Globals::_client_stream = nullptr;
thread_local Zigurat::textstream* Globals::_echo_stream = nullptr;

thread_local bool Globals::_identified = false;
thread_local std::string Globals::_peer_subject;
thread_local std::vector<std::string> Globals::_peer_permissions;

Zigurat::Parser* Globals::_parser = nullptr;
Zigurat::Compiler* Globals::_compiler = nullptr;



bool Globals::reset_mode()
{
  return _reset_mode;
}

bool Globals::trace_mode()
{
  return _trace_mode;
}

bool Globals::permissions_mode()
{
  return _permissions_mode;
}

bool Globals::default_autocommit_mode()
{
  return _default_autocommit_mode;
}

Zigurat::IsolationLevel Globals::default_isolation_level()
{
  return _default_isolation_level;
}

// Everything a compiled object sends goes straight through this pointer, and
// nothing generated checks it first -- so a null one is not a null check that
// was forgotten, it is a call through a vtable at address zero, and the process
// dies with a stack that points at the user's procedure and explains nothing.
//
// A thread running compiled code always has a connection bound to it. If it
// does not, something is wrong that the connection should be told about; and if
// there is no connection to tell, the thread pool logs it and takes the next
// job. Either is better than the process.
//
// echo_stream stays as it is: generated code tests it for null to decide
// whether it is answering a page or a client, so null is an answer there.
Zigurat::binarystream* const Globals::client_stream()
{
  if (_client_stream == nullptr)
    throw Zigurat::ZiguratException(7802, "no connection is bound to this thread");
  return _client_stream;
}

Zigurat::textstream* const Globals::echo_stream()
{
  return _echo_stream;
}

Zigurat::Parser* const Globals::parser()
{
  return _parser;
}

Zigurat::Compiler* const Globals::compiler()
{
  return _compiler;
}

void Globals::set_reset_mode(bool reset_mode)
{
  _reset_mode = reset_mode;
}

void Globals::set_trace_mode(bool trace_mode)
{
  _trace_mode = trace_mode;
}

void Globals::set_permissions_mode(bool permissions_mode)
{
  _permissions_mode = permissions_mode;
}

void Globals::set_default_autocommit_mode(bool default_autocommit_mode)
{
  _default_autocommit_mode = default_autocommit_mode;
}

void Globals::set_default_isolation_level(Zigurat::IsolationLevel default_isolation_level)
{
  _default_isolation_level = default_isolation_level;
}


bool Globals::identified()
{
  return _identified;
}

const std::string& Globals::peer_subject()
{
  return _peer_subject;
}

const std::vector<std::string>& Globals::peer_permissions()
{
  return _peer_permissions;
}

namespace
{
  // "DEMO::AUTHORS" -> {"DEMO", "AUTHORS"}, upper cased. An empty level is
  // dropped rather than treated as a wildcard, so a stray separator cannot
  // widen a permission by accident.
  std::vector<std::string> as_path(const std::string& qualified_name)
  {
    std::vector<std::string> path;
    const std::string name = Zigurat::Utility::to_upper(Zigurat::Utility::trim(qualified_name));

    size_t begin = 0;
    while (begin <= name.size()) {
      size_t end = name.find("::", begin);
      if (end == std::string::npos) end = name.size();
      const std::string level = Zigurat::Utility::trim(name.substr(begin, end - begin));
      if (!level.empty()) path.push_back(level);
      if (end == name.size()) break;
      begin = end + 2;
    }

    return path;
  }
}

bool Globals::permits(const std::string& qualified_name)
{
  // Turned off, a certificate says who its holder is and nothing more. The
  // subject is still known -- the server logs it, and generated code can read
  // it through peer_subject() -- but it decides nothing.
  if (!_permissions_mode) return true;

  // A plain connection has nobody to ask about. Access control arrives with
  // the certificate, so a server without TLS behaves exactly as it always did.
  if (!_identified) return true;

  const std::vector<std::string> object = as_path(qualified_name);
  if (object.empty()) return false;

  for (const std::string& granted : _peer_permissions) {
    const std::vector<std::string> permission = as_path(granted);
    if (permission.empty()) continue;

    if (permission.size() == 1 && permission[0] == "*") return true;

    // A permission covers what it names and everything under it: DEMO reaches
    // every object in the schema, DEMO::AUTHORS reaches only that table.
    if (permission.size() > object.size()) continue;

    bool covers = true;
    for (size_t i = 0; i < permission.size(); i++) {
      if (permission[i] != object[i]) { covers = false; break; }
    }
    if (covers) return true;
  }

  return false;
}

void Globals::require_permission(const std::string& qualified_name)
{
  if (Globals::permits(qualified_name)) return;

  throw Zigurat::ZiguratException(7800, "'" + _peer_subject + "' has no permission for '"
				  + qualified_name + "'");
}

void Globals::set_client_stream(Zigurat::binarystream* client_stream)
{
  _client_stream = client_stream;
}

void Globals::set_peer(const std::string& subject, const std::vector<std::string>& permissions)
{
  _identified = true;
  _peer_subject = subject;
  _peer_permissions = permissions;
}

void Globals::clear_peer()
{
  _identified = false;
  _peer_subject.clear();
  _peer_permissions.clear();
}

void Globals::set_echo_stream(Zigurat::textstream* echo_stream)
{
  _echo_stream = echo_stream;
}

void Globals::set_parser(Zigurat::Parser* parser)
{
  _parser = parser;
}

void Globals::set_compiler(Zigurat::Compiler* compiler)
{
  _compiler = compiler;
}
