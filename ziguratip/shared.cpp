#include <string>
#include <vector>
#include "isolationlevel.hpp"
#include "filestream.hpp"
#include "librarypool.hpp"
#include "globals.hpp"
#include "zexception.hpp"
#include "parser.hpp"
#include "compiler.hpp"


using namespace Zigurat;

// Instance Configuration
extern std::string config_path;
extern std::string locale;
extern std::string home_path;
extern std::string catalog_path;
extern bool        trace_mode;
extern bool        reset_mode;

// Memory 
extern IsolationLevel isolation_level;
extern size_t         memory_page_size;
extern filestream     memory_hexmap_file;
extern filestream     memory_data_file;

// Library Pool
extern std::string            library_path;
extern LibraryPool::CacheMode library_cache_mode;
extern LibraryPool            library_pool;

// Compiler
extern Parser   parser;
extern Compiler compiler;


// What a compiled object will let its caller reach, and whether this connection
// is allowed all of it. The library carries the list itself -- the compiler put
// it there -- so the answer can never drift from the code it describes, the way
// a separate catalogue of grants would.
//
// A library without the symbol was built before permissions existed. It is
// refused rather than waved through: an object nobody can describe is an object
// nobody should be running.
inline void require_objects(LibraryLoader::handle_t handle)
{
  if (!Globals::permissions_mode()) return;
  if (!Globals::identified()) return;

  typedef std::vector<std::string> (*objects_t) (void);

  objects_t objects = nullptr;
  try {
    objects = (objects_t)LibraryLoader::symbol(handle, "objects");
  } catch (const std::exception&) {
    throw ZiguratException(7801, "compiled before permissions existed; recompile it");
  }

  for (const std::string& object : objects())
    Globals::require_permission(object);
}
