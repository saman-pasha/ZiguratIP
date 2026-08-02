#include "ztest.hpp"
#include "configuration.hpp"
#include "argument.hpp"
#include "utility.hpp"
#include <sstream>
#include <list>

using namespace Zigurat;


namespace
{
  // The shape of the shipped ziguratip.conf: indentation is the nesting.
  const char* SAMPLE =
    "LOCALE: en_US.utf8 # a trailing comment\n"
    "RESET_MODE: TRUE\n"
    "\n"
    "TRANSACTION:\n"
    "\tMODE: NON-AUTOCOMMIT\n"
    "\tISOLATION_LEVEL: READ-COMMITTED\n"
    "\n"
    "SERVER:\n"
    "\tTYPE: TCP\n"
    "\tPORT: 2160\n"
    "\tBACKLOG: 5\n";
}


ZTEST(Configuration, reads_top_level_values)
{
  std::stringstream source(SAMPLE);
  Configuration config(source);

  std::string value;
  ZCHECK(config.get("/LOCALE", value));
  ZCHECK_STR(Utility::trim(value), "en_US.utf8");

  ZCHECK(config.get("/RESET_MODE", value));
  ZCHECK_STR(Utility::trim(value), "TRUE");
}

ZTEST(Configuration, reads_nested_values_by_path)
{
  std::stringstream source(SAMPLE);
  Configuration config(source);

  std::string value;
  ZCHECK(config.get("/TRANSACTION/MODE", value));
  ZCHECK_STR(Utility::trim(value), "NON-AUTOCOMMIT");

  ZCHECK(config.get("/TRANSACTION/ISOLATION_LEVEL", value));
  ZCHECK_STR(Utility::trim(value), "READ-COMMITTED");

  ZCHECK(config.get("/SERVER/PORT", value));
  ZCHECK_STR(Utility::trim(value), "2160");
}

ZTEST(Configuration, missing_keys_report_false_and_leave_the_target_alone)
{
  std::stringstream source(SAMPLE);
  Configuration config(source);

  std::string value = "untouched";
  ZCHECK(!config.get("/NO_SUCH_KEY", value));
  ZCHECK_STR(value, "untouched");
  ZCHECK(!config.get("/SERVER/NO_SUCH_CHILD", value));
}

ZTEST(Configuration, comments_are_stripped_from_values)
{
  std::stringstream source(SAMPLE);
  Configuration config(source);

  std::string value;
  config.get("/LOCALE", value);
  ZCHECK(value.find('#') == std::string::npos);
  ZCHECK(value.find("comment") == std::string::npos);
}

ZTEST(Configuration, lists_the_children_of_a_section)
{
  std::stringstream source(SAMPLE);
  Configuration config(source);

  std::list<std::string> children;
  ZCHECK(config.childs("/SERVER", children));
  ZCHECK(children.size() >= 3);
}

ZTEST(Configuration, loads_the_shipped_configuration_file)
{
  // Run from the Test directory or the workspace root, so try both.
  const char* candidates[] = { "../home/etc/ziguratip.conf", "home/etc/ziguratip.conf" };

  bool loaded = false;
  for (int i = 0; i < 2 && !loaded; i++) {
    try {
      std::string path(candidates[i]);
      Configuration config(path);
      std::string value;
      if (config.get("/SERVER/PORT", value)) {
	ZCHECK_STR(Utility::trim(value), "2160");
	loaded = true;
      }
    } catch (...) {
      // try the next path
    }
  }

  ZCHECK(loaded);
}

ZTEST(Configuration, argument_parsing)
{
  const char* argv[] = { "ziguratip", "--config", "/etc/ziguratip.conf", "--trace" };
  Argument args(4, (char**)argv);

  std::string value;
  ZCHECK(args.get("--config", value));
  ZCHECK_STR(value, "/etc/ziguratip.conf");
  ZCHECK(!args.get("--missing", value));
}
