
#ifndef __LIBRARYLOADER_HPP__
#define __LIBRARYLOADER_HPP__

#include <string>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Zigurat
{

  class LibraryLoader
  {
  public:
#if defined(_WIN32) || defined(_WIN64)
    typedef HMODULE handle_t;
    typedef FARPROC symbol_t;
#else
    typedef void* handle_t;
    typedef void* symbol_t;
#endif
    static handle_t handle(std::string);
    static symbol_t symbol(handle_t, std::string);
    static void close(handle_t);
  };

}

#endif // __LIBRARYLOADER_HPP__
