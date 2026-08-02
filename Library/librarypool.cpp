#include "librarypool.h"
#include <iostream>
#include <fstream>
#include <sstream>


namespace Zigurat
{

  LibraryPool::LibraryPool()
    : _cache_mode(CacheMode::NONE)
  {
    
  }

  void LibraryPool::set_cache_mode(CacheMode mode)
  {
    this->_cache_mode = mode;
  }

  LibraryPool::HandlesMap& LibraryPool::_local_handles()
  {
    static thread_local HandlesMap local_handles;
    return local_handles;
  }

  LibraryPool::SymbolsMap& LibraryPool::_local_symbols()
  {
    static thread_local SymbolsMap local_symbols;
    return local_symbols;
  }
  
  void LibraryPool::load_path(std::string path)
  {
    if (this->_cache_mode == CacheMode::GLOBAL) {
#if defined(_WIN32) || defined(_WIN64)
      std::string ls_command = "dir /-n \"" + path + "/*.dll\" > \"" + path + "/libraries\"";
#else
      std::string ls_command = "ls " + path + "/lib*.so >" + path + "/libraries";
#endif
      std::system(ls_command.c_str());
      std::ifstream libs_stream(path + "/libraries");
      std::string lib_name;
	
      while (!libs_stream.eof()) {
	std::getline(libs_stream, lib_name);
#if defined(_WIN32) || defined(_WIN64)
	lib_name = Utility::split(lib_name, ' ')[0];
#endif
	if (lib_name.size() == 0)
	  break;
	std::cout << "Library: '" << lib_name << "'" << std::endl;
	this->handle(lib_name);
      }
    }
  }

  LibraryLoader::handle_t LibraryPool::handle(std::string path)
  {
    if (this->_cache_mode == CacheMode::GLOBAL) {

      typename HandlesMap::iterator iter = this->_global_handles.find(path); 
      if (iter != this->_global_handles.end()) {
	std::cout << "LDLIBRARY: GLOBAL FOUND" << std::endl;
	return iter->second;
      }
      std::cout << "LDLIBRARY: GLOBAL NOT FOUND" << std::endl;
      LibraryLoader::handle_t handle = LibraryLoader::handle(path);
      if (handle)
        this->_global_handles.insert({path, handle});
      return handle;
      
    } else if (this->_cache_mode & CacheMode::LOCAL) {

      typename HandlesMap::iterator iter = this->_local_handles().find(path); 
      if (iter != this->_local_handles().end()) {
	std::cout << "LDLIBRARY: LOCAL FOUND" << std::endl;
	return iter->second;
      }
      LibraryLoader::handle_t handle = LibraryLoader::handle(path);
      if (handle)
        this->_local_handles().insert({path, handle});
      std::cout << "LDLIBRARY: LOCAL NOT FOUND" << std::endl;
      return handle;

    }

    std::cout << "LDLIBRARY: NOT FOUND" << std::endl;
    return LibraryLoader::handle(path);
  }

  LibraryLoader::symbol_t LibraryPool::symbol(LibraryLoader::handle_t handle, std::string sym_name)
  {
    if (this->_cache_mode == CacheMode::GLOBAL) {
    
      auto pair_iter = this->_global_symbols.equal_range(handle); 
      for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {
	if (iter->second.first == sym_name) {
	  std::cout << "LDLIBRARY: GLOBAL SYM FOUND" << std::endl;
	  return iter->second.second;
	}
      }
      std::cout << "LDLIBRARY: GLOBAL NOT SYM FOUND" << std::endl;
      LibraryLoader::symbol_t symbol = LibraryLoader::symbol(handle, sym_name);
      if (symbol)
        this->_global_symbols.insert({handle, {sym_name, symbol}});
      return symbol;
    
    } else if (this->_cache_mode == CacheMode::LOCAL) {

      auto pair_iter = this->_local_symbols().equal_range(handle); 
      for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {
	if (iter->second.first == sym_name) {
	  std::cout << "LDLIBRARY: LOCAL SYM FOUND" << std::endl;
	  return iter->second.second;
	}
      }
      std::cout << "LDLIBRARY: LOCAL NOT SYM FOUND" << std::endl;

      LibraryLoader::symbol_t symbol = LibraryLoader::symbol(handle, sym_name);
      if (symbol)
        this->_local_symbols().insert({handle, {sym_name, symbol}});
      return symbol;
    
    }

    std::cout << "LDLIBRARY: NOT SYM FOUND" << std::endl;
    return LibraryLoader::symbol(handle, sym_name);
  }

  void LibraryPool::close(LibraryLoader::handle_t handle)
  {
    return LibraryLoader::close(handle);
  }

  LibraryPool::~LibraryPool()
  {

  }

  LibraryPool::HandlesMap::~HandlesMap()
  {
    typename HandlesMap::iterator iter = this->begin(); 
    for (; iter != this->end(); iter++)
      LibraryLoader::close(iter->second);
  }

}
