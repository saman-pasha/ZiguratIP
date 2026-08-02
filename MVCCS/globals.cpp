#include "globals.h"
#include "memory.h"


bool Globals::_reset_mode = false;
bool Globals::_trace_mode = false;
bool Globals::_default_autocommit_mode = false;
Zigurat::IsolationLevel Globals::_default_isolation_level = Zigurat::IsolationLevel::READ_COMMITTED;

thread_local Zigurat::binarystream* Globals::_client_stream = nullptr;
thread_local Zigurat::textstream* Globals::_echo_stream = nullptr;

Zigurat::binarystream* Globals::_memory_hexmap_stream = nullptr;
Zigurat::binarystream* Globals::_memory_data_stream = nullptr;
Zigurat::Memory* Globals::_memory = nullptr;
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

bool Globals::default_autocommit_mode()
{
  return _default_autocommit_mode;
}

Zigurat::IsolationLevel Globals::default_isolation_level()
{
  return _default_isolation_level;
}

Zigurat::binarystream* const Globals::client_stream()
{
  return _client_stream;
}

Zigurat::textstream* const Globals::echo_stream()
{
  return _echo_stream;
}

Zigurat::binarystream* const Globals::memory_hexmap_stream()
{
  return _memory_hexmap_stream;
}

Zigurat::binarystream* const Globals::memory_data_stream()
{
  return _memory_data_stream;
}

Zigurat::Memory* const Globals::memory()
{
  return _memory;
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

void Globals::set_default_autocommit_mode(bool default_autocommit_mode)
{
  _default_autocommit_mode = default_autocommit_mode;
}

void Globals::set_default_isolation_level(Zigurat::IsolationLevel default_isolation_level)
{
  _default_isolation_level = default_isolation_level;
}


void Globals::set_client_stream(Zigurat::binarystream* client_stream)
{
  _client_stream = client_stream;
}

void Globals::set_echo_stream(Zigurat::textstream* echo_stream)
{
  _echo_stream = echo_stream;
}

void Globals::set_memory_hexmap_stream(Zigurat::binarystream* memory_hexmap_stream)
{
  _memory_hexmap_stream = memory_hexmap_stream;
}

void Globals::set_memory_data_stream(Zigurat::binarystream* memory_data_stream)
{
  _memory_data_stream = memory_data_stream;
}

void Globals::set_memory(Zigurat::Memory* memory)
{
  _memory = memory;
}

void Globals::set_parser(Zigurat::Parser* parser)
{
  _parser = parser;
}

void Globals::set_compiler(Zigurat::Compiler* compiler)
{
  _compiler = compiler;
}
