
#ifndef __ZTEST_H__
#define __ZTEST_H__

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <functional>

namespace Zigurat
{

  // A deliberately small harness: ZiguratIP has no third party dependencies and
  // the test build should not introduce the first one.
  class ZTest
  {
  public:
    typedef std::function<void()> case_t;

    struct Case
    {
      std::string suite;
      std::string name;
      case_t      body;
    };

    static std::vector<Case>& cases()
    {
      static std::vector<Case> registry;
      return registry;
    }

    static std::string& current_suite()
    {
      static std::string suite;
      return suite;
    }

    static int& checks()   { static int n = 0; return n; }
    static int& failures() { static int n = 0; return n; }

    static std::vector<std::string>& messages()
    {
      static std::vector<std::string> m;
      return m;
    }

    struct Registrar
    {
      Registrar(const std::string& suite, const std::string& name, case_t body)
      {
	ZTest::cases().push_back(Case {suite, name, body});
      }
    };

    static void fail(const std::string& file, int line, const std::string& text)
    {
      ZTest::failures()++;
      std::stringstream ss;
      ss << file << ":" << line << ": " << text;
      ZTest::messages().push_back(ss.str());
    }

    static int run(const std::string& filter);
  };

}

// Every check is counted, and a failing one records where it happened without
// aborting the case, so one broken expectation does not hide the rest.
#define ZCHECK(expr)							\
  do {									\
    Zigurat::ZTest::checks()++;						\
    if (!(expr)) Zigurat::ZTest::fail(__FILE__, __LINE__, "expected: " #expr); \
  } while (0)

#define ZCHECK_EQ(actual, expected)					\
  do {									\
    Zigurat::ZTest::checks()++;						\
    auto _a = (actual);							\
    auto _e = (expected);						\
    if (!(_a == _e)) {							\
      std::stringstream _ss;						\
      _ss << #actual " == " #expected " -- got [" << _a << "] want [" << _e << "]"; \
      Zigurat::ZTest::fail(__FILE__, __LINE__, _ss.str());		\
    }									\
  } while (0)

#define ZCHECK_STR(actual, expected)					\
  do {									\
    Zigurat::ZTest::checks()++;						\
    std::string _a = (actual);						\
    std::string _e = (expected);					\
    if (_a != _e) {							\
      std::stringstream _ss;						\
      _ss << #actual " -- got [" << _a << "] want [" << _e << "]";	\
      Zigurat::ZTest::fail(__FILE__, __LINE__, _ss.str());		\
    }									\
  } while (0)

#define ZCHECK_THROWS(expr)						\
  do {									\
    Zigurat::ZTest::checks()++;						\
    bool _threw = false;						\
    try { expr; } catch (...) { _threw = true; }			\
    if (!_threw) Zigurat::ZTest::fail(__FILE__, __LINE__, "expected a throw: " #expr); \
  } while (0)

#define ZCHECK_NOTHROW(expr)						\
  do {									\
    Zigurat::ZTest::checks()++;						\
    try { expr; }							\
    catch (const std::exception& _e) {					\
      Zigurat::ZTest::fail(__FILE__, __LINE__, std::string("unexpected throw: ") + _e.what()); \
    }									\
    catch (...) {							\
      Zigurat::ZTest::fail(__FILE__, __LINE__, "unexpected throw: " #expr); \
    }									\
  } while (0)

#define ZTEST(suite, name)						\
  static void ztest_##suite##_##name();					\
  static Zigurat::ZTest::Registrar ztest_reg_##suite##_##name		\
    (#suite, #name, ztest_##suite##_##name);				\
  static void ztest_##suite##_##name()

#endif // __ZTEST_H__
