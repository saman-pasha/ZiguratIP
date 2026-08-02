#include <string>
#include "isolationlevel.h"
#include "filestream.h"
#include "librarypool.h"
#include "parser.h"
#include "compiler.h"


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
