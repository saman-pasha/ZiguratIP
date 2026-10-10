#include "ziguratipexception.hpp"
#include "globals.hpp"
#include "filestream.hpp"
#include "mapstream.hpp"
#include "engine.hpp"
#include "configuration.hpp"
#include "shared.cpp"
#include <ctime>
#include <cstring>
#include <fstream>
#include <sstream>


using namespace Zigurat;

Zigurat::IsolationLevel isolation_level = Zigurat::IsolationLevel::READ_COMMITTED;
size_t         memory_page_size = 65536;
// the two store streams: mapped by default (MEMORY/STORE_IO: MAP), a
// filebuf on request (FILE) -- see mapbuf.hpp for what the mapping buys
filestream     memory_hexmap_file;
filestream     memory_data_file;
mapstream      memory_hexmap_map;
mapstream      memory_data_map;
binarystream*  memory_hexmap_stream = nullptr;
binarystream*  memory_data_stream = nullptr;

// THE PAGE IS A PROPERTY OF THE STORE, NOT OF THE CONFIGURATION. The
// engine is handed two streams and a page and believes the page: it counts
// the data file's length over it, pads a ragged last page and frees again
// every page the hexmap does not cover (memory_open in
// MVCCS-cicili/mvccs-lib.cicili). So a store opened at a page it was not
// written at is not refused: it is rewritten.
//
// So a store keeps its page beside it, as text in data/pagesize, written at
// its first open and read back at every later one. /MEMORY/PAGE_SIZE
// chooses the page of a NEW store; a marked store opens at its own page,
// whatever the configuration says now. A store written before the mark
// was written at whatever PAGE_SIZE said then: 8192 unless someone changed
// it, which was this server's default. Opened under 8192 it is marked 8192
// and opens as it always did; under any other page its page cannot be
// known and the open is refused, naming the file that settles it.
//
// The largest page is 65536: a cursor walks a page's hexmap slice, a byte
// for each 16-byte chunk, through a buffer of 4096 bytes
// (cursor_page_hexmap), and a longer page would fail only when a walk
// reached it.
static const size_t memory_page_max = 65536;
static const size_t memory_page_chunk = 16;
static const size_t memory_page_before_mark = 8192;

static std::string store_page_why(size_t page)
{
  if (page == 0)
    return "it is not a positive number";
  if (page > memory_page_max)
    return "it is over 65536, the largest page the engine reads";
  if (page % memory_page_chunk != 0)
    return "it is not a whole number of 16-byte chunks";
  return "";
}

// why TEXT does not name a page, "" when it does and PAGE holds it: one word
// of digits, which store_page_why takes. A sign, a fraction or a suffix is
// not a number here -- a stream would read "-8192" as a huge one and
// "65536abc" as 65536. A number stops growing once it is past the largest
// page, so no run of digits wraps round to a page, and what store_page_why
// is asked is still true of the whole number: zero, over the largest, or
// exactly the number.
static std::string store_page_read(const std::string& text, size_t& page)
{
  std::stringstream words(text);
  std::string word, more;
  page = 0;
  if (!(words >> word))
    return "it is empty";
  if (words >> more)
    return "it is more than one word";
  for (char c : word) {
    if (c < '0' || c > '9')
      return "it is not a number";
    if (page <= memory_page_max)
      page = page * 10 + (size_t)(c - '0');
  }
  return store_page_why(page);
}

// TEXT as a message shows it: without the blanks around it, and cut short
static std::string store_page_shown(const std::string& text)
{
  const char* blank = " \t\r\n\v\f";
  const size_t from = text.find_first_not_of(blank);
  if (from == std::string::npos)
    return "";
  const std::string shown = text.substr(from, text.find_last_not_of(blank) - from + 1);
  return shown.size() > 64 ? shown.substr(0, 64) + "..." : shown;
}

static void store_page_mark(const std::string& path, size_t page)
{
  std::ofstream mark(path, std::ios::out | std::ios::trunc);
  mark << page << std::endl;
  if (!mark.good())
    throw ZiguratIPException("cannot write the store's page mark '" + path + "'; a store whose "
                             "page is not written down cannot be reopened safely");
}

// the page to open the store in DIR at: FRESH when it holds no page yet
static size_t store_page_settle(const std::string& dir, bool fresh, size_t configured)
{
  const std::string path = dir + "/pagesize";
  if (fresh) {
    store_page_mark(path, configured);
    return configured;
  }

  std::ifstream mark(path);
  if (!mark.good()) {
    if (configured == memory_page_before_mark) {
      store_page_mark(path, configured);
      return configured;
    }
    throw ZiguratIPException(
      "the store in '" + dir + "' was written before its page was recorded, so the page it was "
      "written at is not known, and /MEMORY/PAGE_SIZE is " + std::to_string(configured) + ". "
      "Write that page into '" + path + "' -- 8192 if the store was made under the old "
      "default, the PAGE_SIZE it was made with otherwise -- and start again. Opened at "
      "another page, the store would be rewritten, not read.");
  }

  std::stringstream text;
  text << mark.rdbuf();
  size_t page = 0;
  const std::string why = store_page_read(text.str(), page);
  if (!why.empty())
    throw ZiguratIPException("the store's page mark '" + path + "' reads '" + store_page_shown(text.str()) +
                             "', and " + why + ": the store cannot be opened at a page it does not name");
  return page;
}


void load_memory(const Configuration &conf)
{
  clock_t begin_time = clock();

  globals_set_trace_mode(Globals::trace_mode() ? 1 : 0);
  globals_set_reset_mode(Globals::reset_mode() ? 1 : 0);

  std::string value;

  if (conf.get("/TRANSACTION/MODE", value)) {
    value = Utility::to_upper(value);
    if (value == "AUTOCOMMIT")
      Globals::set_default_autocommit_mode(true);
    else if (value == "NON-AUTOCOMMIT")
      Globals::set_default_autocommit_mode(false);
    else
      throw ZiguratIPException("invalid value for '/TRANSACTION/MODE'");
  }
  globals_set_default_autocommit_mode(Globals::default_autocommit_mode() ? 1 : 0);
  std::cout << "Transaction mode: '" << ((Globals::default_autocommit_mode()) ? "AUTOCOMMIT" : "NON-AUTOCOMMIT" ) << "'" << std::endl;

  if (conf.get("/TRANSACTION/ISOLATION_LEVEL", value)) {
    std::string isolation_levelstr = Utility::to_upper(Utility::trim(value));
    if (isolation_levelstr == "READ-UNCOMMITTED")
      isolation_level = Zigurat::IsolationLevel::READ_UNCOMMITTED;
    else if (isolation_levelstr == "READ-COMMITTED")
      isolation_level = Zigurat::IsolationLevel::READ_COMMITTED;
    else if (isolation_levelstr == "REPEATABLE-READ")
      isolation_level = Zigurat::IsolationLevel::REPEATABLE_READ;
    else if (isolation_levelstr == "SNAPSHOT")
      isolation_level = Zigurat::IsolationLevel::SNAPSHOT;
    else if (isolation_levelstr == "SERIALIZABLE")
      isolation_level = Zigurat::IsolationLevel::SERIALIZABLE;
    else
      throw ZiguratIPException("invalid value for '/TRANSACTION/ISOLATION_LEVEL'");
    Globals::set_default_isolation_level(isolation_level);
  }
  // the two enums carry the same members at the same values; the engine's
  // is the one the transactions actually run at
  globals_set_default_isolation_level((::IsolationLevel)(int)Globals::default_isolation_level());
  std::cout << "Transaction isolation level: '" << (int)Globals::default_isolation_level() << "'" << std::endl;

  // the page a NEW store is made with (two older names say the same), read
  // before a store file is touched: a value that names no page is refused
  // with RESET_MODE's truncation not yet done
  size_t configured_page = memory_page_size;
  std::string page_key;
  for (const char* key : { "/MEMORY/PAGE_SIZE", "/MEMORY/MEMORY_PAGE_SIZE", "/MEMORY/BLOCK_SIZE" })
    if (conf.get(key, value)) { page_key = key; break; }
  if (!page_key.empty()) {
    const std::string why = store_page_read(value, configured_page);
    if (!why.empty())
      throw ZiguratIPException("invalid value for '" + page_key + "': '" + store_page_shown(value) + "', and " + why);
  }

  const std::string hexmap_path = home_path + "data/hexmap";
  const std::string data_path = home_path + "data/data";

  // Opening in|out requires the file to already exist, so without this a first
  // run with RESET_MODE FALSE would refuse to start on an empty install. An
  // empty store is a valid one: it initialises to zero pages.
  if (!Globals::reset_mode()) {
    for (const std::string& path : {hexmap_path, data_path}) {
      std::ifstream probe(path);
      if (!probe.good()) {
	std::ofstream create(path, std::ios::binary | std::ios::app);
	if (!create.good())
	  throw ZiguratIPException("cannot create the store file '" + path + "'");
      }
    }
  }

  // A store with no page in it yet takes the configured one, measured before
  // the streams open: RESET_MODE empties the data file there, and a data file
  // of no bytes holds no page whatever its page was (the mapped stream leaves
  // a file exactly as long as what was written).
  bool store_fresh = Globals::reset_mode();
  if (!store_fresh) {
    std::ifstream probe(data_path, std::ios::binary | std::ios::ate);
    store_fresh = probe.good() && probe.tellg() == 0;
  }

  const std::ios_base::openmode store_mode = std::ios::in | std::ios::out | std::ios::binary |
    (Globals::reset_mode() ? std::ios::trunc : (std::ios_base::openmode)0);

  bool mapped = true;
  if (conf.get("/MEMORY/STORE_IO", value)) {
    value = Utility::to_upper(Utility::trim(value));
    if (value == "MAP") mapped = true;
    else if (value == "FILE") mapped = false;
    else throw ZiguratIPException("invalid value for '/MEMORY/STORE_IO'");
  }
  if (mapped) {
    memory_hexmap_map.open(hexmap_path, store_mode);
    memory_data_map.open(data_path, store_mode);
    if (!memory_hexmap_map.good())
      throw ZiguratIPException("invalid hexmap file");
    if (!memory_data_map.good())
      throw ZiguratIPException("invalid data file");
    memory_hexmap_stream = &memory_hexmap_map;
    memory_data_stream = &memory_data_map;
  } else {
    memory_hexmap_file.open(hexmap_path, store_mode);
    memory_data_file.open(data_path, store_mode);
    if (!memory_hexmap_file.good())
      throw ZiguratIPException("invalid hexmap file");
    if (!memory_data_file.good())
      throw ZiguratIPException("invalid data file");
    memory_hexmap_stream = &memory_hexmap_file;
    memory_data_stream = &memory_data_file;
  }
  std::cout << "Store I/O: '" << (mapped ? "MAP" : "FILE") << "'" << std::endl;

  std::cout << "Hexmap file: '" << hexmap_path << "'" << std::endl;
  std::cout << "Data file: '" << data_path << "'" << std::endl;

  // The Cicili engine: one Memory for the process, opaque behind
  // libMVCCS. Compiled objects reach it through globals_memory() --
  // that is what engine-compat.hpp's Globals::memory() forwards to --
  // and each table attaches its own indexes on first touch, so nothing
  // here wires a catalogue index the way the old engine did.
  // AND THE STORE'S BYTE ORDER IS THIS MACHINE'S, checked before a byte of
  // it is parsed. The store is written in host order (filestream and
  // mapstream are hbostream; only the protocol is normalised), so one
  // carried from a machine of the other kind would open on its byte-array
  // page keys and then read every int64 reversed. Refused by name instead.
  {
    char order_err[512] = { 0 };
    const std::string store_dir = home_path + "data";
    if (!store_order_check(store_dir.c_str(), order_err, sizeof order_err))
      throw ZiguratIPException(order_err);
  }

  memory_page_size = store_page_settle(home_path + "data", store_fresh, configured_page);
  std::cout << "Memory page size: '" << memory_page_size << "'";
  if (memory_page_size != configured_page)
    std::cout << " (the store's own, from data/pagesize; a new store's would be "
              << configured_page << ")";
  std::cout << std::endl;

  ::Memory* engine_memory = engine_memory_new();
  memory_open(engine_memory, memory_hexmap_stream, memory_data_stream, (int64_t)memory_page_size);
  globals_set_memory(engine_memory);

  // Parallel reads are THE DEFAULT: read-only cursors take the streams guard
  // shared and read through per-thread streams instead of queueing on the one
  // canonical pair. ZIGURATIP_PARALLEL_READS=0 keeps the exclusive guard, so
  // one env var separates the two modes in any future bisect.
  {
    const char* par_reads = std::getenv("ZIGURATIP_PARALLEL_READS");
    if (par_reads == nullptr || std::strcmp(par_reads, "0") != 0) {
      memory_reader_paths(engine_memory, hexmap_path.c_str(), data_path.c_str());
      std::cout << "Parallel reads: on" << std::endl;
    } else {
      std::cout << "Parallel reads: off (ZIGURATIP_PARALLEL_READS=0)" << std::endl;
    }
  }

  clock_t end_time = clock();

  std::cout << "Memory initialization time : " << double(end_time - begin_time) / CLOCKS_PER_SEC << " s" << std::endl;
}
