#include "ztest.hpp"
#include "session.hpp"
#include "httprequest.hpp"
#include "httpresponse.hpp"
#include "httpserver.hpp"
#include "rpcpool.hpp"
#include "bufferstream.hpp"
#include "types.hpp"
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

using namespace Zigurat;


namespace
{
  // A request/response pair standing in for one HTTP exchange.
  class Exchange
  {
  private:
    bufferstream _stream;
    std::mutex _access;
    std::condition_variable _semaphore;
    volatile size_t _dispatch = 1;

  public:
    HTTPRequest* request;
    HTTPResponse* response;

    explicit Exchange(const std::string& cookie_header = "")
    {
      std::map<std::string, std::string> headers;
      headers["HOST"] = "127.0.0.1:2190";
      if (!cookie_header.empty()) headers["COOKIE"] = cookie_header;

      this->request = new HTTPRequest("GET", "/index.zt", "HTTP/1.1", headers,
				      "127.0.0.1", "2190", nullptr, 0);
      this->response = new HTTPResponse(this->request, 1, this->_dispatch,
					this->_stream, this->_access, this->_semaphore);
    }

    // What the visitor's browser would send back on the next request.
    std::string cookie_echo()
    {
      const std::string id = this->response->COOKIE(String(Session::COOKIE_NAME)).value();
      if (id.empty()) return "";
      return Session::COOKIE_NAME + "=" + id;
    }

    ~Exchange()
    {
      delete this->response;
      delete this->request;
    }
  };

  // Every case starts from an empty store and an unbound thread.
  struct Clean
  {
    Clean()  { Session::EXPIRE_ALL(); Session::RELEASE(); }
    ~Clean() { Session::EXPIRE_ALL(); Session::RELEASE(); }
  };
}


ZTEST(Session, starts_unbound)
{
  Clean clean;
  ZCHECK(!Session::IS_INITIALIZED().value());
  ZCHECK_STR(Session::ID().value(), "");
}

ZTEST(Session, initialize_mints_an_id_and_sets_the_cookie)
{
  Clean clean;
  Exchange visit;

  Session::INITIALIZE(*visit.request, *visit.response);

  ZCHECK(Session::IS_INITIALIZED().value());
  const std::string id = Session::ID().value();
  ZCHECK(id.size() > 0);

  // The cookie the visitor gets back names that session.
  ZCHECK(visit.response->HAS_COOKIE(String(Session::COOKIE_NAME)).value());
  ZCHECK_STR(visit.response->COOKIE(String(Session::COOKIE_NAME)).value(), id);
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);
}

ZTEST(Session, the_cookie_carries_the_session_to_the_next_request)
{
  Clean clean;

  std::string first_id, echo;
  {
    Exchange first;
    Session::INITIALIZE(*first.request, *first.response);
    first_id = Session::ID().value();
    Session::SET<String>(String("message"), String("Hello World!"));
    echo = first.cookie_echo();
    Session::RELEASE();
  }

  ZCHECK(!Session::IS_INITIALIZED().value());   // the thread is free again

  {
    Exchange second(echo);
    Session::INITIALIZE(*second.request, *second.response);

    ZCHECK_STR(Session::ID().value(), first_id);          // same session
    ZCHECK_STR(Session::GET<String>(String("message")).to_std_string(), "Hello World!");
    ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);   // not a second one
  }
}

// What a session is actually for: one page puts something away, a different
// page takes it out again, and the only thing joining them is the visitor's
// cookie.
//
// The two pages here are two handlers rather than two compiled .parsi objects --
// what is being tested is the store and the cookie that carries it, and those
// are the same whichever page calls them. Each begins with INITIALIZE, because
// every page must, and ends with RELEASE, because the request scope does.
ZTEST(Session, a_value_set_by_one_page_is_read_by_another)
{
  Clean clean;

  std::string cookie;

  // Page one: a form handler that remembers who the visitor said they are.
  {
    Exchange page_one;
    Session::INITIALIZE(*page_one.request, *page_one.response);

    Session::SET<String>(String("who"), String("Sadegh Hedayat"));
    Session::SET<Int>(String("visits"), Int(1));

    cookie = page_one.cookie_echo();     // what the browser would keep
    Session::RELEASE();
  }

  // Between the two, this worker thread is bound to nobody -- as it would be
  // while it served somebody else entirely.
  ZCHECK(!Session::IS_INITIALIZED().value());

  // Page two: a different page, same visitor, carrying the cookie back.
  {
    Exchange page_two(cookie);
    Session::INITIALIZE(*page_two.request, *page_two.response);

    ZCHECK_STR(Session::GET<String>(String("who")).to_std_string(), "Sadegh Hedayat");
    ZCHECK_EQ(Session::GET<Int>(String("visits")).value(), 1);

    // and it can write back for the page after it
    Session::SET<Int>(String("visits"), Int(2));
    Session::RELEASE();
  }

  // Page three, to show the update stuck rather than the first value lingering.
  {
    Exchange page_three(cookie);
    Session::INITIALIZE(*page_three.request, *page_three.response);
    ZCHECK_EQ(Session::GET<Int>(String("visits")).value(), 2);
    Session::RELEASE();
  }

  // One visitor, one session, however many pages they walked through.
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);
}

// A visitor who arrives with no cookie is a different visitor, and must not see
// what the first one put away.
ZTEST(Session, another_visitor_does_not_see_it)
{
  Clean clean;

  {
    Exchange mine;
    Session::INITIALIZE(*mine.request, *mine.response);
    Session::SET<String>(String("who"), String("Sadegh Hedayat"));
    Session::RELEASE();
  }

  {
    Exchange theirs;                     // no cookie: a fresh browser
    Session::INITIALIZE(*theirs.request, *theirs.response);
    ZCHECK(!Session::HAS(String("who")).value());
    Session::RELEASE();
  }

  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)2);   // two visitors
}

// Anyone can send any cookie, so an id the server never issued must not be
// adopted -- otherwise a caller picks its own session identifier.
ZTEST(Session, a_forged_cookie_does_not_take_over_a_session)
{
  Clean clean;

  Exchange forged(Session::COOKIE_NAME + "=deadbeefdeadbeefdeadbeefdeadbeefdeadbeef");
  Session::INITIALIZE(*forged.request, *forged.response);

  ZCHECK(Session::ID().value() != "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef");
  ZCHECK(Session::ID().value().size() > 0);
  // A replacement was issued, so the cookie is rewritten.
  ZCHECK(forged.response->HAS_COOKIE(String(Session::COOKIE_NAME)).value());
}

ZTEST(Session, ids_are_unique)
{
  Clean clean;
  std::set<std::string> ids;

  for (int i = 0; i < 50; i++) {
    Exchange visit;
    Session::INITIALIZE(*visit.request, *visit.response);
    ids.insert(Session::ID().value());
    Session::RELEASE();
  }

  ZCHECK_EQ(ids.size(), (size_t)50);
  // A digest, not a counter: nothing short or predictable.
  ZCHECK(ids.begin()->size() >= 32);
}

ZTEST(Session, values_round_trip_by_type)
{
  Clean clean;
  Exchange visit;
  Session::INITIALIZE(*visit.request, *visit.response);

  Session::SET<String>(String("who"), String("ZiguratIP"));
  Session::SET<Int>(String("count"), Int(42));
  Session::SET<Long>(String("big"), Long((int64_t)9000000000LL));
  Session::SET<Bool>(String("flag"), Bool(true));

  ZCHECK_STR(Session::GET<String>(String("who")).to_std_string(), "ZiguratIP");
  ZCHECK_EQ(Session::GET<Int>(String("count")).value(), 42);
  ZCHECK_EQ(Session::GET<Long>(String("big")).value(), (int64_t)9000000000LL);
  ZCHECK(Session::GET<Bool>(String("flag")).value());
}

ZTEST(Session, a_missing_key_reads_as_null)
{
  Clean clean;
  Exchange visit;
  Session::INITIALIZE(*visit.request, *visit.response);

  ZCHECK(!Session::HAS(String("absent")).value());
  ZCHECK(Session::GET<String>(String("absent")).is_null().value());
}

ZTEST(Session, has_remove_and_clear)
{
  Clean clean;
  Exchange visit;
  Session::INITIALIZE(*visit.request, *visit.response);

  Session::SET<Int>(String("a"), Int(1));
  Session::SET<Int>(String("b"), Int(2));
  ZCHECK(Session::HAS(String("a")).value());

  Session::REMOVE(String("a"));
  ZCHECK(!Session::HAS(String("a")).value());
  ZCHECK(Session::HAS(String("b")).value());

  Session::CLEAR();
  ZCHECK(!Session::HAS(String("b")).value());
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);   // the session itself remains
}

ZTEST(Session, sessions_cannot_read_each_others_values)
{
  Clean clean;

  Exchange one;
  Session::INITIALIZE(*one.request, *one.response);
  Session::SET<String>(String("secret"), String("one"));
  const std::string one_echo = one.cookie_echo();
  Session::RELEASE();

  Exchange two;
  Session::INITIALIZE(*two.request, *two.response);
  ZCHECK(!Session::HAS(String("secret")).value());
  Session::SET<String>(String("secret"), String("two"));
  Session::RELEASE();

  Exchange one_again(one_echo);
  Session::INITIALIZE(*one_again.request, *one_again.response);
  ZCHECK_STR(Session::GET<String>(String("secret")).to_std_string(), "one");

  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)2);
}

// Writing without a session must be inert rather than landing somewhere.
// Using a session nobody asked for is a mistake in the page, and it says so.
//
// This used to be writing_while_unbound_is_ignored, and ignored was the wrong
// answer: a page that forgets INITIALIZE set values that vanished, read back
// nothing, and reported success. The request looked fine and the data was never
// there. Reads and writes throw now; asking whether there is a session, and
// which one, still answer plainly, because those are the questions a page asks
// before deciding.
ZTEST(Session, using_a_session_while_unbound_throws)
{
  Clean clean;

  ZCHECK_THROWS(Session::SET<Int>(String("orphan"), Int(1)));
  ZCHECK_THROWS(Session::HAS(String("orphan")));
  ZCHECK_THROWS(Session::REMOVE(String("orphan")));
  ZCHECK_THROWS(Session::CLEAR());
  ZCHECK_THROWS(Session::DESTROY());

  // and nothing was created on the way past
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)0);

  // the questions a page asks first still answer
  ZCHECK_NOTHROW(Session::IS_INITIALIZED());
  ZCHECK(!Session::IS_INITIALIZED().value());
  ZCHECK_NOTHROW(Session::ID());
  ZCHECK_STR(Session::ID().value(), "");

  // and releasing when there is nothing to release is not an error: the request
  // scope does it on every request, bound or not.
  ZCHECK_NOTHROW(Session::RELEASE());
}

ZTEST(Session, release_keeps_the_session_but_destroy_removes_it)
{
  Clean clean;

  Exchange visit;
  Session::INITIALIZE(*visit.request, *visit.response);
  Session::RELEASE();
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);
  ZCHECK(!Session::IS_INITIALIZED().value());

  Exchange again(visit.cookie_echo());
  Session::INITIALIZE(*again.request, *again.response);
  Session::DESTROY();
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)0);
  ZCHECK(!Session::IS_INITIALIZED().value());
}

ZTEST(Session, idle_sessions_expire)
{
  Clean clean;

  Exchange visit;
  Session::INITIALIZE(*visit.request, *visit.response);
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);

  Session::EXPIRE(3600);                                        // still fresh
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);

  Session::EXPIRE(0);                                           // disabled, no sweep
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);

  Session::EXPIRE(-1);                                          // also disabled
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)1);
}

// The thread pool hands consecutive requests from one visitor to different
// threads, so the store has to be shared and guarded.
ZTEST(Session, concurrent_requests_keep_their_own_sessions)
{
  Clean clean;

  const int THREADS = 8;
  const int ROUNDS = 25;
  std::atomic<int> mismatches(0);
  std::atomic<int> completed(0);

  std::vector<std::thread> workers;
  for (int t = 0; t < THREADS; t++) {
    workers.push_back(std::thread([&, t] () {
	  std::string echo;
	  for (int r = 0; r < ROUNDS; r++) {
	    Exchange visit(echo);
	    Session::INITIALIZE(*visit.request, *visit.response);

	    if (r == 0) {
	      Session::SET<Int>(String("owner"), Int(t));
	      echo = visit.cookie_echo();
	    } else {
	      // Every later request must land back in this worker's session.
	      Int owner = Session::GET<Int>(String("owner"));
	      if (owner.is_null().value() || owner.value() != t) mismatches++;
	    }

	    Session::RELEASE();
	  }
	  completed++;
	}));
  }

  for (size_t i = 0; i < workers.size(); i++) workers[i].join();

  ZCHECK_EQ(completed.load(), THREADS);
  ZCHECK_EQ(mismatches.load(), 0);
  ZCHECK_EQ((uint64_t)Session::COUNT().value(), (uint64_t)THREADS);
}


// ---------------------------------------------------------------------------
// The request parser behind Zeytun.
// ---------------------------------------------------------------------------

ZTEST(Zeytun, request_exposes_its_parts)
{
  std::map<std::string, std::string> headers;
  headers["HOST"] = "example.org:2190";
  headers["USER-AGENT"] = "curl/8";

  HTTPRequest request("GET", "/pages/index.zt?name=zigurat&n=7", "HTTP/1.1",
		      headers, "example.org", "2190", nullptr, 0);

  ZCHECK_STR(request.METHOD().to_std_string(), "GET");
  ZCHECK_STR(request.PROTOCOL().to_std_string(), "HTTP/1.1");
  ZCHECK_STR(request.HOST().to_std_string(), "example.org");
  ZCHECK_STR(request.PORT().to_std_string(), "2190");
  ZCHECK_STR(request.PATH().to_std_string(), "/pages/index.zt");
  ZCHECK(request.HAS_HEADER(String("USER-AGENT")).value());
  ZCHECK(!request.HAS_HEADER(String("AUTHORIZATION")).value());
}

ZTEST(Zeytun, query_string_is_parsed)
{
  std::map<std::string, std::string> headers;
  headers["HOST"] = "example.org";

  HTTPRequest request("GET", "/index.zt?name=zigurat&n=7", "HTTP/1.1",
		      headers, "example.org", "", nullptr, 0);

  ZCHECK(request.HAS_QUERY(String("name")).value());
  ZCHECK_STR(request.QUERY(String("name")).to_std_string(), "zigurat");
  ZCHECK_STR(request.QUERY(String("n")).to_std_string(), "7");
  ZCHECK(!request.HAS_QUERY(String("missing")).value());
}

ZTEST(Zeytun, cookies_are_parsed_from_the_header)
{
  std::map<std::string, std::string> headers;
  headers["HOST"] = "example.org";
  headers["COOKIE"] = "first=one; second=two";

  HTTPRequest request("GET", "/", "HTTP/1.1", headers, "example.org", "", nullptr, 0);

  ZCHECK(request.HAS_COOKIE(String("first")).value());
  ZCHECK_STR(request.COOKIE(String("first")).to_std_string(), "one");
  ZCHECK_STR(request.COOKIE(String("second")).to_std_string(), "two");
  ZCHECK(!request.HAS_COOKIE(String("third")).value());
}

ZTEST(Zeytun, response_headers_and_cookies)
{
  Exchange visit;

  visit.response->SET_HEADER(String("Content-Type"), String("text/html"));
  ZCHECK(visit.response->HAS_HEADER(String("Content-Type")).value());
  ZCHECK_STR(visit.response->HEADER(String("Content-Type")).to_std_string(), "text/html");

  visit.response->REMOVE_HEADER(String("Content-Type"));
  ZCHECK(!visit.response->HAS_HEADER(String("Content-Type")).value());

  visit.response->SET_COOKIE(String("track"), String("42"));
  ZCHECK(visit.response->HAS_COOKIE(String("track")).value());
  ZCHECK_STR(visit.response->COOKIE(String("track")).to_std_string(), "42");
}

// What a proxy sends, and what happens to a refusal nobody can see.
namespace
{
  // Drives one request through the server's own head parser and hands back
  // what the handler was given, plus whatever went onto the connection.
  struct Served
  {
    std::string method, uri, path, host, port;
    std::string raw;
    bool reached_handler = false;
  };

  Served serve(const std::string& request)
  {
    Served served;
    bufferstream stream;
    stream.write(request.data(), (std::streamsize)request.size());
    stream.seekg(0, std::ios::beg);

    HTTPServer::handle_client(stream,
      [&served] (binarystream*, HTTPRequest* request, HTTPResponse* response) {
	std::unique_ptr<HTTPRequest> request_deleter(request);
	std::unique_ptr<HTTPResponse> response_deleter(response);
	served.reached_handler = true;
	served.method = request->METHOD().value();
	served.uri    = request->URI().value();
	served.path   = request->PATH().value();
	served.host   = request->HOST().value();
	served.port   = request->PORT().value();
      },
      false, 0, false, 8000, 16000, 8 * 1024 * 1024);

    stream.clear();
    stream.seekg(0, std::ios::beg);
    served.raw = stream.string();
    return served;
  }
}

// RFC 7230 section 5.3.2: a server must accept a request line naming the whole
// URL, not only the path. Proxies send it. This took the target as it stood, so
// the path became "/http:/host:2190/page" and every proxied request 404'd.
ZTEST(Zeytun, an_absolute_form_request_line_is_the_path_it_names)
{
  Served origin   = serve("GET /catalog.zt HTTP/1.1\r\nHost: example:2190\r\nConnection: close\r\n\r\n");
  Served absolute = serve("GET http://example:2190/catalog.zt HTTP/1.1\r\nHost: example:2190\r\nConnection: close\r\n\r\n");

  ZCHECK(origin.reached_handler);
  ZCHECK(absolute.reached_handler);
  ZCHECK_STR(absolute.uri, origin.uri);
  ZCHECK_STR(absolute.path, origin.path);
}

// The authority in the target stands in for a missing Host, which the same
// section says to prefer -- and this server refuses a request without one.
ZTEST(Zeytun, an_absolute_form_request_line_carries_its_own_host)
{
  Served served = serve("GET http://example:2190/catalog.zt HTTP/1.1\r\nConnection: close\r\n\r\n");

  ZCHECK(served.reached_handler);
  ZCHECK_STR(served.host, "example");
  ZCHECK_STR(served.port, "2190");
}

// A refusal with no body is a blank page: a browser shows nothing, view-source
// shows nothing, and a missing page looks exactly like a broken server.
ZTEST(Zeytun, a_refusal_says_what_it_was)
{
  Served served = serve("BREW /coffee HTTP/1.1\r\nHost: example\r\nConnection: close\r\n\r\n");

  ZCHECK(!served.reached_handler);                                  // refused before any page
  ZCHECK(served.raw.find("501 Not Implemented") != std::string::npos);
  ZCHECK(served.raw.find("Content-Type: text/plain") != std::string::npos);
  ZCHECK(served.raw.find("Content-Length: 0") == std::string::npos);  // never an empty refusal

  // and the body is actually there, after the blank line
  const size_t split = served.raw.find("\r\n\r\n");
  ZCHECK(split != std::string::npos);
  ZCHECK(served.raw.size() > split + 4);
}

// How this server knows who is asking, which has exactly two answers and they
// are not equally strong.
ZTEST(Session, a_certificate_names_a_user_and_a_cookie_names_a_browser)
{
  // A certificate subject is authenticated: the peer proved it holds the key,
  // against an authority this server trusts, and it is checked again on every
  // request. That is a person, and they are the same person in two browsers.
  ZCHECK_STR(RPCPool::owner("C=US, CN=alice", ""), "peer:C=US, CN=alice");

  // It wins over a cookie, so opening a second browser does not open a second
  // identity for somebody who has a certificate.
  ZCHECK_STR(RPCPool::owner("C=US, CN=alice", "abc123"), "peer:C=US, CN=alice");

  // With no certificate there is nobody to be sure about and the cookie is all
  // there is. It names a browser: whoever holds it is that session.
  ZCHECK_STR(RPCPool::owner("", "abc123"), "session:abc123");

  // The two are kept apart, so a certificate subject cannot be crafted to read
  // as somebody's session id and inherit what it owns.
  ZCHECK(RPCPool::owner("abc123", "") != RPCPool::owner("", "abc123"));

  // Neither, and there is no owner to speak of. Silence here would mean a
  // transaction owned by nobody, which is how the thread-local version behaved.
  ZCHECK_THROWS(RPCPool::owner("", ""));
}
