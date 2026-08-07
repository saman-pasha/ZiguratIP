#include "session.hpp"
#include "zexception.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"
#include "shahelper.hpp"
#include "utility.hpp"
#include <random>
#include <sstream>


namespace Zigurat
{

  const std::string Session::COOKIE_NAME = "ZIPSESSID";

  std::map<std::string, Session::Store> Session::_store;
  std::mutex Session::_store_access;
  thread_local std::string Session::_current;


  // Utility::generate_id() hashes the thread id and the clock, so two requests
  // served by the same pooled thread in the same second get the same value.
  // A session id has to be unique and unguessable, hence a real random source
  // and a digest rather than that.
  //
  // The random source used to be a std::mt19937_64 seeded once, at static
  // initialisation, from a single std::random_device{}() -- 64 bits of seed
  // behind a generator that is not one-way. Mersenne Twister state is
  // recoverable from its own output, so a client who collected a couple of
  // session ids could reconstruct the generator and name every id the server
  // would hand out afterwards, to any user. The digest hid the outputs but the
  // state was still only 64 bits of guessing away. Now each id is drawn
  // straight from the kernel pool, and the counter and clock remain only to
  // guarantee uniqueness if two draws ever collide.
  std::string Session::_new_id()
  {
    static uint64_t counter = 0;

    std::stringstream seed;
    {
      std::lock_guard<std::mutex> lock(Session::_store_access);
      seed << Utility::random_uint64() << ':' << Utility::random_uint64() << ':' << ++counter;
    }
    seed << ':' << (uint64_t)std::time(0);

    return Utility::to_lower(SHA::checksum(SHA::SHA1, seed.str()));
  }

  void Session::INITIALIZE(HTTPRequest& request, HTTPResponse& response)
  {
    std::string id;

    if (request.HAS_COOKIE(String(Session::COOKIE_NAME)).value())
      id = Utility::trim(request.COOKIE(String(Session::COOKIE_NAME)).value());

    const std::time_t now = std::time(0);

    std::unique_lock<std::mutex> lock(Session::_store_access);

    // An id has to name a session this server actually issued; anything else
    // is discarded rather than trusted, so a caller cannot pick its own.
    if (id.empty() || Session::_store.find(id) == Session::_store.end()) {
      lock.unlock();
      id = Session::_new_id();
      lock.lock();

      Store fresh;
      fresh.created = now;
      fresh.accessed = now;
      Session::_store[id] = fresh;

      lock.unlock();
      response.SET_COOKIE(String(Session::COOKIE_NAME), String(id));
      response.SET_COOKIE_ATTRIBUTE(String(Session::COOKIE_NAME), String("Path"), String("/"));
      response.SET_COOKIE_ATTRIBUTE(String(Session::COOKIE_NAME), String("HttpOnly"));
      response.SET_COOKIE_ATTRIBUTE(String(Session::COOKIE_NAME), String("SameSite"), String("Lax"));
      lock.lock();
    } else {
      Session::_store[id].accessed = now;
    }

    Session::_current = id;
  }

  String Session::ID()
  {
    return String(Session::_current);
  }

  Bool Session::IS_INITIALIZED()
  {
    return Bool(!Session::_current.empty());
  }

  void Session::_set_raw(const std::string& key, const std::string& value)
  {
    // Reading or writing a session that was never bound used to return quietly,
    // and quietly is the wrong answer: a page that forgets INITIALIZE then sets
    // values that vanish, reads back nothing, and reports success. Nothing in
    // the request fails, so the bug surfaces later as data that was never
    // there. A page must ask for a session before it has one.
    if (Session::_current.empty())
      throw ZiguratException(7806, "no session: call Session::INITIALIZE(request, response) before using one");


    std::lock_guard<std::mutex> lock(Session::_store_access);
    std::map<std::string, Store>::iterator it = Session::_store.find(Session::_current);
    if (it == Session::_store.end()) return;

    it->second.values[key] = value;
    it->second.accessed = std::time(0);
  }

  bool Session::_get_raw(const std::string& key, std::string& value)
  {
    // Reading or writing a session that was never bound used to return quietly,
    // and quietly is the wrong answer: a page that forgets INITIALIZE then sets
    // values that vanish, reads back nothing, and reports success. Nothing in
    // the request fails, so the bug surfaces later as data that was never
    // there. A page must ask for a session before it has one.
    if (Session::_current.empty())
      throw ZiguratException(7806, "no session: call Session::INITIALIZE(request, response) before using one");


    std::lock_guard<std::mutex> lock(Session::_store_access);
    std::map<std::string, Store>::iterator it = Session::_store.find(Session::_current);
    if (it == Session::_store.end()) return false;

    std::map<std::string, std::string>::const_iterator entry = it->second.values.find(key);
    if (entry == it->second.values.end()) return false;

    value = entry->second;
    it->second.accessed = std::time(0);
    return true;
  }

  Bool Session::HAS(String key)
  {
    std::string ignored;
    return Bool(Session::_get_raw(key.value(), ignored));
  }

  void Session::REMOVE(String key)
  {
    // Reading or writing a session that was never bound used to return quietly,
    // and quietly is the wrong answer: a page that forgets INITIALIZE then sets
    // values that vanish, reads back nothing, and reports success. Nothing in
    // the request fails, so the bug surfaces later as data that was never
    // there. A page must ask for a session before it has one.
    if (Session::_current.empty())
      throw ZiguratException(7806, "no session: call Session::INITIALIZE(request, response) before using one");


    std::lock_guard<std::mutex> lock(Session::_store_access);
    std::map<std::string, Store>::iterator it = Session::_store.find(Session::_current);
    if (it != Session::_store.end()) it->second.values.erase(key.value());
  }

  void Session::CLEAR()
  {
    // Reading or writing a session that was never bound used to return quietly,
    // and quietly is the wrong answer: a page that forgets INITIALIZE then sets
    // values that vanish, reads back nothing, and reports success. Nothing in
    // the request fails, so the bug surfaces later as data that was never
    // there. A page must ask for a session before it has one.
    if (Session::_current.empty())
      throw ZiguratException(7806, "no session: call Session::INITIALIZE(request, response) before using one");


    std::lock_guard<std::mutex> lock(Session::_store_access);
    std::map<std::string, Store>::iterator it = Session::_store.find(Session::_current);
    if (it != Session::_store.end()) it->second.values.clear();
  }

  void Session::RELEASE()
  {
    Session::_current.clear();
  }

  void Session::DESTROY()
  {
    // Reading or writing a session that was never bound used to return quietly,
    // and quietly is the wrong answer: a page that forgets INITIALIZE then sets
    // values that vanish, reads back nothing, and reports success. Nothing in
    // the request fails, so the bug surfaces later as data that was never
    // there. A page must ask for a session before it has one.
    if (Session::_current.empty())
      throw ZiguratException(7806, "no session: call Session::INITIALIZE(request, response) before using one");


    {
      std::lock_guard<std::mutex> lock(Session::_store_access);
      Session::_store.erase(Session::_current);
    }
    Session::_current.clear();
  }

  void Session::DESTROY(HTTPResponse& response)
  {
    if (!Session::_current.empty())
      response.REMOVE_COOKIE(String(Session::COOKIE_NAME));
    Session::DESTROY();
  }

  ULong Session::COUNT()
  {
    std::lock_guard<std::mutex> lock(Session::_store_access);
    return ULong((uint64_t)Session::_store.size());
  }

  void Session::EXPIRE(std::time_t max_idle_seconds)
  {
    if (max_idle_seconds <= 0) return;

    const std::time_t now = std::time(0);
    std::lock_guard<std::mutex> lock(Session::_store_access);

    for (std::map<std::string, Store>::iterator it = Session::_store.begin(); it != Session::_store.end(); ) {
      if (now - it->second.accessed >= max_idle_seconds)
	Session::_store.erase(it++);
      else
	++it;
    }
  }

  void Session::EXPIRE_ALL()
  {
    std::lock_guard<std::mutex> lock(Session::_store_access);
    Session::_store.clear();
  }

}
