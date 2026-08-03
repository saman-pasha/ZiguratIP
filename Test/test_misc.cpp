#include "ztest.hpp"
#include "zlibhelper.hpp"
#include "tokenizer.hpp"
#include "token.hpp"
#include "libraryloader.hpp"
#include "librarypool.hpp"
#include "connector.hpp"
#include "resulttype.hpp"
#include "typeint.hpp"
#include "typestring.hpp"
#include "typelong.hpp"
#include "bufferstream.hpp"
#include <list>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>

using namespace Zigurat;


// ---------------------------------------------------------------------------
// Compression
// ---------------------------------------------------------------------------

ZTEST(Compression, deflate_roundtrip)
{
  std::string data;
  for (int i = 0; i < 500; i++) data += "ZiguratIP compresses well when it repeats. ";

  bufferstream input(data);
  bufferstream compressed;
  ZLib::compress(ZLib::DEFLATE, input, (std::streamsize)data.size(), compressed);

  const std::string packed = compressed.string();
  ZCHECK(packed.size() > 0);
  ZCHECK(packed.size() < data.size());   // repetitive input must actually shrink

  bufferstream to_expand(packed);
  bufferstream expanded;
  ZLib::decompress(ZLib::DEFLATE, to_expand, (std::streamsize)packed.size(), expanded);

  ZCHECK_STR(expanded.string(), data);
}

// Only DEFLATE is wired up; the ZLIB and GZIP wrappers are declared in the enum
// but still throw. Pinned here so finishing them shows up as a failing case.
ZTEST(Compression, zlib_and_gzip_are_not_implemented_yet)
{
  const std::string data = "a short payload";

  bufferstream gzip_input(data);
  bufferstream gzip_output;
  ZCHECK_THROWS(ZLib::compress(ZLib::GZIP, gzip_input, (std::streamsize)data.size(), gzip_output));

  bufferstream zlib_input(data);
  bufferstream zlib_output;
  ZCHECK_THROWS(ZLib::compress(ZLib::ZLIB, zlib_input, (std::streamsize)data.size(), zlib_output));
}

ZTEST(Compression, binary_payload_roundtrip)
{
  std::string binary;
  for (int i = 0; i < 1024; i++) binary.push_back((char)(i % 256));

  bufferstream input(binary);
  bufferstream compressed;
  ZLib::compress(ZLib::DEFLATE, input, (std::streamsize)binary.size(), compressed);

  const std::string packed = compressed.string();
  bufferstream to_expand(packed);
  bufferstream expanded;
  ZLib::decompress(ZLib::DEFLATE, to_expand, (std::streamsize)packed.size(), expanded);

  ZCHECK(expanded.string() == binary);
}


// ---------------------------------------------------------------------------
// Compiler front end
// ---------------------------------------------------------------------------

namespace
{
  size_t count_of(const std::list<Token>& tokens, TokenType type)
  {
    size_t n = 0;
    for (std::list<Token>::const_iterator it = tokens.begin(); it != tokens.end(); ++it)
      if (it->type == type) n++;
    return n;
  }
}

ZTEST(Compiler, tokenizes_names_and_operators)
{
  std::list<Token> tokens;
  ZCHECK_NOTHROW(Tokenizer::tokenize("SELECT name FROM person", tokens));

  ZCHECK(tokens.size() > 0);
  ZCHECK(count_of(tokens, TokenType::NAME) >= 4);
}

ZTEST(Compiler, tokenizes_numeric_literals)
{
  std::list<Token> tokens;
  Tokenizer::tokenize("42 3.5", tokens);

  ZCHECK(count_of(tokens, TokenType::INT) >= 1);
  ZCHECK(count_of(tokens, TokenType::FLOAT) >= 1);
}

ZTEST(Compiler, tokenizes_string_literals)
{
  std::list<Token> tokens;
  Tokenizer::tokenize("name = 'ZiguratIP'", tokens);

  ZCHECK(count_of(tokens, TokenType::STR) >= 1);
  ZCHECK(count_of(tokens, TokenType::OP) >= 1);
}

ZTEST(Compiler, tokenizes_parentheses)
{
  std::list<Token> tokens;
  Tokenizer::tokenize("f(a, b)", tokens);

  ZCHECK(count_of(tokens, TokenType::LPAR) >= 1);
  ZCHECK(count_of(tokens, TokenType::RPAR) >= 1);
}

ZTEST(Compiler, records_source_positions)
{
  std::list<Token> tokens;
  Tokenizer::tokenize("alpha\nbeta", tokens);

  ZCHECK(tokens.size() >= 2);
  if (tokens.size() >= 2) {
    std::list<Token>::const_iterator it = tokens.begin();
    const int first_line = it->line_no;
    ++it;
    ZCHECK(it->line_no >= first_line);
  }
}

ZTEST(Compiler, empty_input_yields_no_content_tokens)
{
  std::list<Token> tokens;
  ZCHECK_NOTHROW(Tokenizer::tokenize("", tokens));
  ZCHECK_EQ(count_of(tokens, TokenType::NAME), (size_t)0);
}

ZTEST(Compiler, character_classes_are_populated)
{
  ZCHECK(Tokenizer::DIGITS.size() > 0);
  ZCHECK(Tokenizer::ALPHABETS.size() > 0);
  ZCHECK(Tokenizer::OPERATORS.size() > 0);
  ZCHECK(Tokenizer::SPACES.size() > 0);
}


// ---------------------------------------------------------------------------
// Library loader
// ---------------------------------------------------------------------------

ZTEST(Library, loads_a_shared_object_and_finds_a_symbol)
{
  // Load one of ZiguratIP's own libraries rather than shipping a fixture.
  const char* candidates[] = {
    "../home/lib/libCore.so", "home/lib/libCore.so",
    "../home/lib/libCore.dylib", "home/lib/libCore.dylib"
  };

  LibraryLoader::handle_t handle = nullptr;
  for (int i = 0; i < 4 && handle == nullptr; i++) {
    try { handle = LibraryLoader::handle(candidates[i]); }
    catch (...) { handle = nullptr; }
  }

  ZCHECK(handle != nullptr);

  if (handle != nullptr) {
    // Zigurat::Utility::htons(uint16_t), which libCore definitely exports.
    // dlsym takes the name without the leading underscore nm shows.
    LibraryLoader::symbol_t symbol = nullptr;
    ZCHECK_NOTHROW(symbol = LibraryLoader::symbol(handle, "_ZN7Zigurat7Utility5htonsEt"));
    ZCHECK(symbol != nullptr);

    if (symbol != nullptr) {
      uint16_t (*htons_fn)(uint16_t) = (uint16_t (*)(uint16_t))symbol;
      const uint16_t net = htons_fn(0x0102);
      uint8_t octets[2];
      std::memcpy(octets, &net, 2);
      ZCHECK_EQ((int)octets[0], 0x01);
    }

    ZCHECK_NOTHROW(LibraryLoader::close(handle));
  }
}

ZTEST(Library, missing_library_is_rejected)
{
  ZCHECK_THROWS(LibraryLoader::handle("/nonexistent/path/libNope.so"));
}

ZTEST(Library, pool_accepts_a_cache_mode)
{
  LibraryPool pool;
  ZCHECK_NOTHROW(pool.set_cache_mode(LibraryPool::NONE));
  ZCHECK_NOTHROW(pool.set_cache_mode(LibraryPool::LOCAL));
  ZCHECK_NOTHROW(pool.set_cache_mode(LibraryPool::GLOBAL));
}


// ---------------------------------------------------------------------------
// Connector
// ---------------------------------------------------------------------------

ZTEST(Connector, refuses_to_connect_to_a_dead_server)
{
  // Port 1 on loopback has nothing listening, so open() must fail cleanly
  // rather than hang or crash.
  Connector connector;
  ZCHECK_THROWS(connector.open("127.0.0.1", "1", true, 2));
  ZCHECK(!connector.is_open());
}

// The full client/server round trip against a live ziguratip. Start one with
// Test/run-e2e.sh, or this case reports itself as skipped.
ZTEST(Connector, round_trip_against_a_live_server)
{
  const char* service = std::getenv("ZIGURATIP_TEST_SERVICE");
  if (service == nullptr) service = "2160";

  Connector connector;

  try {
    connector.open("127.0.0.1", service, true, 10);
  } catch (...) {
    std::cout << "          (skipped: no server listening on " << service << ")" << std::endl;
    return;
  }

  ZCHECK(connector.is_open());
  ZCHECK(connector.transaction_id() != 0);

  // Each echo is a request the buffer has to push before it blocks on the reply.
  ZCHECK_STR(connector.echo("hello from the connector"), "hello from the connector");
  ZCHECK_STR(connector.echo("second round trip"), "second round trip");
  ZCHECK_STR(connector.echo(""), "");

  ZCHECK_NOTHROW(connector.auto_commit(true));
  ZCHECK_NOTHROW(connector.commit());

  ZCHECK_NOTHROW(connector.close());
  ZCHECK(!connector.is_open());
}

// A connection holds one worker thread, and the transaction is that thread's,
// so it lives for as long as the connection does. Several calls therefore belong
// to one transaction and the client decides its fate: commit and all of them
// stand, roll back and none of them do.
//
// This needs procedures to call, so it runs only where the demo has been built.
ZTEST(Connector, a_transaction_spans_calls_until_the_client_ends_it)
{
  const char* service = std::getenv("ZIGURATIP_TEST_SERVICE");
  if (service == nullptr) service = "2160";

  // How many books there are, counted in a transaction of its own.
  auto count_books = [service] () -> int64_t {
    Connector reader;
    reader.open("127.0.0.1", service, true, 10);
    int64_t rows = 0;
    reader.call("demo::count_books");
    Long total(0);
    for (ResultType r = reader.result(); r != ResultType::SUCCESSFUL_DONE; r = reader.result()) {
      if (r == ResultType::CURSOR_OPEN)       reader.columns();
      else if (r == ResultType::RETURN_VALUE) reader.fetch(total);
    }
    rows = total.is_null().value() ? -1 : total.value();
    reader.close();
    return rows;
  };

  auto add_three = [service] (const std::string& tag, bool commit) {
    Connector writer;
    writer.open("127.0.0.1", service, true, 10);
    for (int n = 1; n <= 3; n++) {
      writer.call("demo::add_book");
      writer.write_string(String(tag + " " + std::to_string(n)));
      writer.write_long(Long((int64_t)1));
      writer.write_int(Int(2026));
      Long id(0);
      for (ResultType r = writer.result(); r != ResultType::SUCCESSFUL_DONE; r = writer.result()) {
	if (r == ResultType::CURSOR_OPEN)       writer.columns();
	else if (r == ResultType::RETURN_VALUE) writer.fetch(id);
      }
    }
    if (commit) writer.commit(); else writer.rollback();
    writer.close();
  };

  int64_t before = 0;
  try {
    before = count_books();
  } catch (...) {
    std::cout << "          (skipped: no server, or the demo is not built)" << std::endl;
    return;
  }
  if (before < 0) { std::cout << "          (skipped: demo::count_books is unavailable)" << std::endl; return; }

  // Rolled back: none of the three survive.
  ZCHECK_NOTHROW(add_three("rolled back", false));
  ZCHECK_EQ(count_books(), before);

  // Committed: all three do, and the client committed them, not the procedure.
  ZCHECK_NOTHROW(add_three("committed", true));
  ZCHECK_EQ(count_books(), before + 3);
}
