// parsic -- compile Parsi source on a running Zigurat, over the connector.
//
// WHY THIS EXISTS RATHER THAN `parsi'. They are two different things:
//
//   parsi   reads ziguratip.conf and compiles HERE, writing the .so, the
//           header and the catalogue entry into this machine's home tree.
//   parsic  reads connector.conf and asks a SERVER to compile, so the object
//           lands in the server's tree and the running server has it. Nothing
//           is written locally at all.
//
// The editor wants the second: an object compiled into a home directory no
// server is reading does not answer a request. It is also the only one that
// works when the server is not on this machine.
//
// EXIT STATUS IS THE POINT. e2e-probe can already speak the `compile' verb and
// deliberately always exits 0, because the shell scripts around it compare a
// line under `set -e'. An editor needs the opposite: non-zero when the compile
// failed, and the server's diagnostic on stderr, so compilation-mode can tell
// success from failure without parsing prose.

#include "connector.hpp"
#include "connectorexception.hpp"
#include "zexception.hpp"
#include "utility.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Zigurat;

namespace
{
  void help()
  {
    std::cout
      << "\tZiguratIP Parsi remote compiler" << std::endl
      << std::endl
      << "Usage:" << std::endl
      << "\tparsic <file.parsi> [<file.parsi> ...]" << std::endl
      << "\tparsic --config" << std::endl
      << "\tparsic --help" << std::endl
      << std::endl
      << "Arguments:" << std::endl
      << "\t<file.parsi>   ::= source to compile, in the order given" << std::endl
      << "\t--config       ::= print the connector.conf that would be used," << std::endl
      << "\t                    and nothing else. Empty output and status 1" << std::endl
      << "\t                    mean there is none." << std::endl
      << "\t--help | -h    ::= this" << std::endl
      << std::endl
      << "Where it connects:" << std::endl
      << "\tWhatever connector.conf says, looked for in this order:" << std::endl
      << "\t  1. $ZIGURATIP_HOME/etc/connector.conf" << std::endl
      << "\t  2. ~/ZiguratIP/etc/connector.conf" << std::endl
#if defined(_WIN32) || defined(_WIN64)
      << "\t  3. %PROGRAMDATA%/ZiguratIP/connector.conf" << std::endl
#else
      << "\t  3. /etc/ZiguratIP/connector.conf" << std::endl
#endif
      << std::endl
      << "Exit status:" << std::endl
      << "\t0  every file compiled" << std::endl
      << "\t1  something did not -- the reason is on stderr" << std::endl
      << std::endl
      << "The server must allow it: a compile arriving over the network is" << std::endl
      << "refused unless COMPILER/REMOTE_MODE is TRUE in ziguratip.conf, and it is" << std::endl
      << "off by default. See doc/security.md." << std::endl;
  }

  std::string read_all(const std::string& path)
  {
    std::ifstream source(path);
    if (!source.good()) throw ConnectorException("cannot read " + path);
    std::ostringstream all;
    all << source.rdbuf();
    return all.str();
  }
}

int main(int argc, char** argv)
{
  std::vector<std::string> files;

  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg == "--help" || arg == "-h") { help(); return 0; }
    if (arg == "--config") {
      // The path only. An editor opens what this prints, and prose on stdout
      // would be a file name it then failed to find.
      std::string path = Utility::config_path("connector.conf");
      if (path.empty()) {
	std::cerr << "parsic: no connector.conf found" << std::endl;
	return 1;
      }
      std::cout << path << std::endl;
      return 0;
    }
    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "parsic: unknown option " << arg << std::endl;
      return 1;
    }
    files.push_back(arg);
  }

  if (files.empty()) { help(); return 1; }

  try {
    // EVERY FILE IS READ BEFORE ANYTHING IS SENT. A misspelled path is the
    // caller's mistake and needs no server to diagnose; connecting first
    // reported it as "tcp connect failed", which sends the reader to look at
    // the wrong thing entirely. It also means a batch that cannot be completed
    // is not started halfway.
    std::vector<std::string> sources;
    sources.reserve(files.size());
    for (const std::string& file : files) sources.push_back(read_all(file));

    Connector connector;

    // ONE CONNECTION FOR ALL THE FILES, and they are compiled in the order
    // given. That matters: an object REQUIREs the objects it links against and
    // they have to exist first, which is why demo/ is numbered.
    connector.open();

    for (size_t i = 0; i < files.size(); i++) {
      try {
	connector.compile(sources[i]);
      } catch (const ZiguratException& e) {
	// CAUGHT PER FILE so the name can be put in front of the message. The
	// server says "syntax error at line 4 column 3 near 'RETRN'" and has no
	// idea what the caller called the file, so without this an editor has a
	// line number and nothing to open.
	std::cerr << Utility::diagnostic(files[i], e.what()) << std::endl;
	return 1;
      }

      // Named, because a compile of several files that stops halfway has to say
      // where. The editor shows this and it is the only record of how far it got.
      std::cout << files[i] << ": compiled" << std::endl;
    }

    connector.close();
    return 0;

  } catch (const ZiguratException& e) {
    // The server's own diagnostic, verbatim -- it names the line and column,
    // and rewording it here would only lose that.
    std::cerr << "parsic: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "parsic: " << e.what() << std::endl;
    return 1;
  }
}
