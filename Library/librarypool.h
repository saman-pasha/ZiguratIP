
#ifndef __LIBRARYPOOL_H__
#define __LIBRARYPOOL_H__

#include "libraryloader.h"
#include <string>
#include <thread>
#include <map>

namespace Zigurat
{

  class LibraryPool
  {
  public:
    enum CacheMode {
      NONE,
      GLOBAL,
      LOCAL
    };

    class HandlesMap : public std::map<std::string, LibraryLoader::handle_t>
    {
    public:
      using std::map<std::string, LibraryLoader::handle_t>::map;
      virtual ~HandlesMap();
    };

    typedef std::multimap<LibraryLoader::handle_t, std::pair<std::string, LibraryLoader::symbol_t> > SymbolsMap;

  private:
    CacheMode _cache_mode;
    HandlesMap _global_handles;
    SymbolsMap _global_symbols;
    static HandlesMap& _local_handles();
    static SymbolsMap& _local_symbols();
  public:
    LibraryPool();
    void set_cache_mode(CacheMode);
    void load_path(std::string);
    LibraryLoader::handle_t handle(std::string);
    LibraryLoader::symbol_t symbol(LibraryLoader::handle_t, std::string);
    void close(LibraryLoader::handle_t);
    virtual ~LibraryPool();
  };

}

#endif // __LIBRARYPOOL_H__
