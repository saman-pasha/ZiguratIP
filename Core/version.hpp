#ifndef __VERSION_HPP__
#define __VERSION_HPP__

namespace Zigurat
{

  // ZIGURATIP'S VERSION, AND THE ONLY PLACE IT IS WRITTEN. Core is linked
  // by every library and every binary in the workspace, so this one
  // function is what the server, parsi, parsic and ca all answer with --
  // and `--version' prints it alone on stdout, so V=$(ziguratip --version)
  // reads a bare number with nothing to strip. A second copy anywhere (a
  // -D on a compile line, a string in a banner, a number in a document) is
  // a second thing to forget, and the forgotten one is the one a reader
  // believes. The rule for moving it is beside the literal, in version.cpp.
  const char* version();

}

#endif
