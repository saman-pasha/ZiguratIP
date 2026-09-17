#include "version.hpp"

namespace Zigurat
{

  // THE NUMBER. Bump it in the SAME COMMIT as the change it describes,
  // never afterwards.
  //
  // THE PATCH IS THE DEFAULT: an ordinary change moves it, and it keeps
  // moving. The MINOR is for something new a caller can reach -- a
  // statement the Parsi compiler now takes, an option a binary now
  // answers, an entry point a library now offers. The MAJOR is for
  // something that worked and no longer does: a format a store cannot
  // open, a signature a generated object cannot link, a statement that
  // stopped meaning what it meant. Neither of those two is TAKEN -- it is
  // proposed, with what changed observably, and the owner decides; getting
  // it wrong the timid way costs nothing, getting it wrong the loud way
  // tells every reader downstream that something broke when nothing did.
  //
  // A DOCUMENTATION-ONLY COMMIT DOES NOT BUMP AT ALL: there is no new
  // binary for the number to describe.
  //
  // Test/run-version.sh pins the SHAPE and deliberately not the number --
  // a test that named it would be a second place to edit, and the one
  // somebody forgets.
  const char* version() { return "0.1.10"; }

}
