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


using namespace Zigurat;

Zigurat::IsolationLevel isolation_level = Zigurat::IsolationLevel::READ_COMMITTED;
size_t         memory_page_size = 8192;
// the two store streams: mapped by default (MEMORY/STORE_IO: MAP), a
// filebuf on request (FILE) -- see mapbuf.hpp for what the mapping buys
filestream     memory_hexmap_file;
filestream     memory_data_file;
mapstream      memory_hexmap_map;
mapstream      memory_data_map;
binarystream*  memory_hexmap_stream = nullptr;
binarystream*  memory_data_stream = nullptr;


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

  if (conf.get("/MEMORY/PAGE_SIZE", value) || conf.get("/MEMORY/MEMORY_PAGE_SIZE", value) ||
      conf.get("/MEMORY/BLOCK_SIZE", value)) {
    std::stringstream bsss(value);
    bsss >> memory_page_size;
  }
  std::cout << "Memory page size: '" << memory_page_size << "'" << std::endl;

  // The Cicili engine: one Memory for the process, opaque behind
  // libMVCCS. Compiled objects reach it through globals_memory() --
  // that is what engine-compat.hpp's Globals::memory() forwards to --
  // and each table attaches its own indexes on first touch, so nothing
  // here wires a catalogue index the way the old engine did.
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
