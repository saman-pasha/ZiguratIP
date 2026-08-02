#include "libraryloader.h"
#include <iostream>
#include <stdexcept>


namespace Zigurat 
{
  
  LibraryLoader::handle_t LibraryLoader::handle(std::string ld_path)
  {
#if defined(_WIN32) || defined(_WIN64)
    LibraryLoader::handle_t handle = LoadLibrary(ld_path.c_str());
#else
    LibraryLoader::handle_t handle = dlopen(ld_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#endif
    if (handle) {
      return handle;
    } else {
#if defined(_WIN32) || defined(_WIN64)
      throw std::runtime_error(GetLastError());
#else
      throw std::runtime_error(dlerror());
#endif
    }
  }

  LibraryLoader::symbol_t LibraryLoader::symbol(LibraryLoader::handle_t handle, std::string sym_name)
  {
#if defined(_WIN32) || defined(_WIN64)
    LibraryLoader::symbol_t symbol = GetProcAddress(handle, sym_name.c_str());
#else
    LibraryLoader::symbol_t symbol = dlsym(handle, sym_name.c_str());
#endif
    if (symbol) {
      return symbol;
    } else {
#if defined(_WIN32) || defined(_WIN64)
      throw std::runtime_error(GetLastError());
#else
      throw std::runtime_error(dlerror());
#endif
    }
  }

  void LibraryLoader::close(LibraryLoader::handle_t handle)
  {
#if defined(_WIN32) || defined(_WIN64)
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
  }

}
