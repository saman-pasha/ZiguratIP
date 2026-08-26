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
extern Zigurat::IsolationLevel isolation_level;
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
extern bool     compiler_remote_mode;


// Which copy of the storage engine a compiled object ended up bound to.
//
// A compiled object names the core libraries as dependencies, and the loader
// resolves those names again every time one is opened. Rebuild home/lib while
// the server is running -- a make, an install, a package upgrade -- and the
// file behind ../home/lib/libMVCCS.so is no longer the one the server mapped at
// startup, so the loader maps it a second time and binds the object to that.
//
// Nothing complains. The object loads, every symbol resolves, and then it
// writes its rows through a second Globals whose client stream was never set,
// which is a null pointer, and the server dies inside the user's procedure.
// The only cure is a restart, so say so before running any of it.
extern "C" const void* mvccs_runtime_instance();

inline void require_runtime(LibraryLoader::handle_t handle)
{
#if defined(_WIN32) || defined(_WIN64)
  // None of this can happen on Windows, and the question cannot be asked there
  // either. A mapped DLL is locked, so the file behind a library the server is
  // holding cannot be replaced underneath it -- the rebuild fails at the linker,
  // where somebody is watching. And GetProcAddress reads a module's own exports
  // and never its imports, so there would be no way to find out which storage
  // engine an object ended up bound to.
  (void)handle;
#else
  typedef const void* (*instance_t) (void);

  instance_t instance = nullptr;
  try {
    // Through the library's own dependencies, which is the whole point: this
    // finds whichever libMVCCS2 that object was bound to, not ours.
    instance = (instance_t)LibraryLoader::symbol(handle, "mvccs_runtime_instance");
  } catch (const std::exception&) {
    throw ZiguratException(7803, "this object is not bound to a storage engine this server knows;"
			   " the libraries under it have changed, restart the server");
  }

  if (instance() != mvccs_runtime_instance())
    throw ZiguratException(7804, "this object is bound to a second copy of the storage engine:"
			   " the libraries were replaced while the server was running,"
			   " restart it");
#endif
}

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
