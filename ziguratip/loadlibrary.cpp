#include "ziguratipexception.hpp"
#include "librarypool.hpp"
#include "configuration.hpp"
#include "utility.hpp"
#include "shared.cpp"


using namespace Zigurat;

std::string            library_path;
LibraryPool::CacheMode library_cache_mode;
LibraryPool            library_pool;


void load_library(const Configuration &config)
{
  std::string value = "";

  library_path = home_path + "ld";
  config.get("/LIBRARY_PATH", library_path);
  std::cout << "Library path: '" << library_path << "'" << std::endl;

  if (config.get("/LIBRARY/CACHE_MODE", value)) {
    value = Utility::to_upper(value);
    if (value == "NONE")
      library_cache_mode = LibraryPool::NONE;
    else if (value == "GLOBAL")
      library_cache_mode = LibraryPool::GLOBAL;
    else if (value == "LOCAL")
      library_cache_mode = LibraryPool::LOCAL;
    else
      throw ZiguratIPException("invalid value for '/LIBRARY/LOCAL_CACHE_MODE'");
  }
  std::cout << "Library cache mode: '" << (int)library_cache_mode << "'" << std::endl;

  library_pool.set_cache_mode(library_cache_mode);
  library_pool.load_path(library_path);
}
