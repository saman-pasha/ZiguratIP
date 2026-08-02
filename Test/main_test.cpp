#include "ztest.hpp"
#include <map>
#include <ctime>
#include <cstring>
#include <iomanip>
#include <algorithm>

namespace Zigurat
{

  int ZTest::run(const std::string& filter)
  {
    std::vector<Case>& registry = ZTest::cases();

    std::stable_sort(registry.begin(), registry.end(),
		     [] (const Case& l, const Case& r) { return l.suite < r.suite; });

    std::string suite;
    int suite_failures = 0;
    int suite_cases = 0;
    int total_cases = 0;
    int failed_cases = 0;
    int skipped = 0;

    std::map<std::string, int> per_suite_failures;

    for (size_t i = 0; i < registry.size(); i++) {
      const Case& c = registry[i];

      if (!filter.empty() && c.suite != filter && c.name.find(filter) == std::string::npos) {
	skipped++;
	continue;
      }

      if (c.suite != suite) {
	if (!suite.empty())
	  std::cout << "  " << suite_cases << " case(s), "
		    << suite_failures << " failed" << std::endl << std::endl;
	suite = c.suite;
	suite_failures = 0;
	suite_cases = 0;
	std::cout << "[" << suite << "]" << std::endl;
      }

      const int before = ZTest::failures();
      suite_cases++;
      total_cases++;

      try {
	c.body();
      } catch (const std::exception& e) {
	ZTest::fail("<case>", 0, c.suite + "." + c.name + " escaped: " + e.what());
      } catch (...) {
	ZTest::fail("<case>", 0, c.suite + "." + c.name + " escaped with a foreign exception");
      }

      const int delta = ZTest::failures() - before;
      if (delta > 0) {
	failed_cases++;
	suite_failures += delta;
	per_suite_failures[c.suite] += delta;
	std::cout << "  FAIL  " << c.name << std::endl;
	for (size_t m = ZTest::messages().size() - delta; m < ZTest::messages().size(); m++)
	  std::cout << "          " << ZTest::messages()[m] << std::endl;
      } else {
	std::cout << "  ok    " << c.name << std::endl;
      }
    }

    if (!suite.empty())
      std::cout << "  " << suite_cases << " case(s), "
		<< suite_failures << " failed" << std::endl << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "cases    : " << total_cases << " run, " << failed_cases << " failed";
    if (skipped > 0) std::cout << ", " << skipped << " filtered out";
    std::cout << std::endl;
    std::cout << "checks   : " << ZTest::checks() << " run, " << ZTest::failures() << " failed" << std::endl;

    if (!per_suite_failures.empty()) {
      std::cout << "failing  :";
      for (std::map<std::string, int>::const_iterator it = per_suite_failures.begin();
	   it != per_suite_failures.end(); ++it)
	std::cout << " " << it->first << "(" << it->second << ")";
      std::cout << std::endl;
    }

    std::cout << "result   : " << ((ZTest::failures() == 0) ? "PASS" : "FAIL") << std::endl;
    std::cout << "========================================" << std::endl;

    return (ZTest::failures() == 0) ? 0 : 1;
  }

}

int main(int argc, char** argv)
{
  std::string filter = (argc > 1) ? argv[1] : "";
  return Zigurat::ZTest::run(filter);
}
