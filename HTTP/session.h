
#ifndef __SESSION_H__
#define __SESSION_H__

#include "typestring.h"
#include "typebool.h"
#include "typeulong.h"
#include "bufferstream.h"
#include <string>
#include <map>
#include <mutex>
#include <ctime>

namespace Zigurat
{

  class HTTPRequest;
  class HTTPResponse;

  // A virtual session carried by an HTTP cookie. The store lives in the
  // process and is shared by every worker thread, because the thread pool
  // hands consecutive requests from one visitor to different threads. Only
  // the binding "which session is this thread currently serving" is
  // thread local, and Zeytun clears it when the request ends.
  class Session
  {
  public:
    static const std::string COOKIE_NAME;

    struct Store
    {
      std::map<std::string, std::string> values;
      std::time_t created = 0;
      std::time_t accessed = 0;
    };

    // Binds the calling thread to the session named by the request cookie,
    // minting a new one and setting the cookie when the visitor has none.
    static void INITIALIZE(HTTPRequest&, HTTPResponse&);

    static String ID();
    static Bool IS_INITIALIZED();

    static Bool HAS(String);
    static void REMOVE(String);
    static void CLEAR();

    // Ends this thread's association with the session. The session itself
    // stays in the store for the visitor's next request.
    static void RELEASE();

    // Discards the whole session, cookie included where a response is given.
    static void DESTROY();
    static void DESTROY(HTTPResponse&);

    static ULong COUNT();
    static void EXPIRE(std::time_t);
    static void EXPIRE_ALL();

    // Values are held in their packed binarystream form, so anything with the
    // stream operators round trips.
    template <typename T>
    static void SET(String key, const T& value)
    {
      bufferstream packed;
      packed << value;
      Session::_set_raw(key.value(), packed.string());
    }

    template <typename T>
    static T GET(String key)
    {
      std::string raw;
      T value(nullptr);
      if (!Session::_get_raw(key.value(), raw)) return value;
      bufferstream packed(raw);
      packed >> value;
      return value;
    }

  private:
    static std::map<std::string, Store> _store;
    static std::mutex _store_access;
    static thread_local std::string _current;

    static std::string _new_id();
    static void _set_raw(const std::string&, const std::string&);
    static bool _get_raw(const std::string&, std::string&);
  };

}

#endif // __SESSION_H__
