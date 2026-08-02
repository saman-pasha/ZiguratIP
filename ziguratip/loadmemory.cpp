#include "ziguratipexception.h"
#include "globals.h"
#include "filestream.h"
#include "memory.h"
#include "btreerecord.h"
#include "btreeindex.h"
#include "configuration.h"
#include "shared.cpp"
#include <ctime>
#include <fstream>


using namespace Zigurat;

IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;
size_t         memory_page_size = 8192;
filestream     memory_hexmap_file;
filestream     memory_data_file;


void load_memory(const Configuration &conf)
{
  clock_t begin_time = clock();

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
  std::cout << "Transaction mode: '" << ((Globals::default_autocommit_mode()) ? "AUTOCOMMIT" : "NON-AUTOCOMMIT" ) << "'" << std::endl;

  if (conf.get("/TRANSACTION/ISOLATION_LEVEL", value)) {
    std::string isolation_levelstr = Utility::to_upper(Utility::trim(value));
    if (isolation_levelstr == "READ-UNCOMMITTED")
      isolation_level = IsolationLevel::READ_UNCOMMITTED;
    else if (isolation_levelstr == "READ-COMMITTED")
      isolation_level = IsolationLevel::READ_COMMITTED;
    else if (isolation_levelstr == "REPEATABLE-READ")
      isolation_level = IsolationLevel::REPEATABLE_READ;
    else if (isolation_levelstr == "SNAPSHOT")
      isolation_level = IsolationLevel::SNAPSHOT;
    else if (isolation_levelstr == "SERIALIZABLE")
      isolation_level = IsolationLevel::SERIALIZABLE;
    else
      throw ZiguratIPException("invalid value for '/TRANSACTION/ISOLATION_LEVEL'");
    Globals::set_default_isolation_level(isolation_level);
  }
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

  memory_hexmap_file.open(hexmap_path, store_mode);
  memory_data_file.open(data_path, store_mode);

  if (!memory_hexmap_file.good())
    throw ZiguratIPException("invalid hexmap file");
  if (!memory_data_file.good())
    throw ZiguratIPException("invalid data file");
  
  std::cout << "Hexmap file: '" << hexmap_path << "'" << std::endl;
  std::cout << "Data file: '" << data_path << "'" << std::endl;
  
  Globals::set_memory_hexmap_stream(&memory_hexmap_file);
  Globals::set_memory_data_stream(&memory_data_file);
  
  if (conf.get("/MEMORY/PAGE_SIZE", value) || conf.get("/MEMORY/MEMORY_PAGE_SIZE", value) ||
      conf.get("/MEMORY/BLOCK_SIZE", value)) {
    std::stringstream bsss(value);
    bsss >> memory_page_size;
  }
  std::cout << "Memory page size: '" << memory_page_size << "'" << std::endl;

  Globals::set_memory(new Memory(*Globals::memory_hexmap_stream(),
				 *Globals::memory_data_stream(),
				 memory_page_size));

  BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = new BTreeIndex<BTreeRecord, String>
    (Globals::memory(), "IDX_ZIGURAT_BTREERECORD_HASH_NAME", true, BTreeRecord::hash_name);

  Globals::memory()->commit_transaction();

  clock_t end_time = clock();

  std::cout << "Memory initialization time : " << double(end_time - begin_time) / CLOCKS_PER_SEC << " s" << std::endl;
}
