
#ifndef __RPCPOOL_HPP__
#define __RPCPOOL_HPP__

#include "connector.hpp"
#include <memory>
#include <string>

namespace Zigurat
{

  // Connections that outlive the request that opened them.
  //
  // A page that lets a visitor open a transaction, do some work, and commit it
  // later has a problem HTTP does not solve for it: those are three requests,
  // and the connection carrying the transaction has to still be there for the
  // second and third. The RPC console kept it in a thread local, which meant the
  // transaction belonged to whichever worker served the request -- two visitors
  // sharing a worker shared a transaction, and either could commit the other's
  // uncommitted work.
  //
  // So a connection is addressed by the transaction it carries, and remembers
  // who opened it. A visitor may hold as many as they like at once, and they are
  // different connections, so their transactions run in parallel rather than
  // queueing behind one another.
  class RPCPool
  {
  public:
    // Who is asking, as well as this server can tell.
    //
    // Over mutual TLS that is the subject of the certificate they presented,
    // checked on every request against the authority, with no cookie and no
    // login -- that is a *user*, and the same user is the same identity in two
    // different browsers.
    //
    // With no certificate there is nothing to authenticate, and the best
    // available handle is the session cookie. That identifies a *browser*:
    // whoever holds the cookie is that session. It is prefixed so a crafted
    // certificate subject cannot be made to collide with a session id.
    //
    // It is a pure function of the two identities rather than something that
    // fetches them, because this library is built before HTTP and MVCCS and
    // cannot reach Session or Globals. The page supplies both -- it has them --
    // and the choice between them stays here, where it can be tested.
    static std::string owner(const std::string& peer_subject, const std::string& session_id);

    // Connects, and hands back the transaction id its connection carries. That
    // id is what everything below is addressed by.
    static std::string open(const std::string& owner);

    // The connection that transaction names. Throws if there is no such
    // transaction, or if it belongs to somebody else -- which is the case the
    // thread local could not tell apart from your own.
    //
    // Shared rather than a reference, because Parsi has no way to name an
    // existing object -- it cannot declare a reference, return one, or
    // dereference a pointer. A handle it can copy is something it can hold, so
    // the Parsi wrapper keeps one of these and stays copyable.
    static std::shared_ptr<Connector> held(const std::string& transaction, const std::string& owner);

    // The connection a handle names.
    //
    // Parsi cannot dereference: it has no prefix * and no ->, so
    // shared_ptr::get() hands it a pointer it has no way to use. This gives back
    // the reference, which Parsi can call methods on directly. It is the whole
    // reason the wrapper can hold a connection rather than being one.
    static Connector& ref(const std::shared_ptr<Connector>&);

    // Closes and forgets it. Commits nothing: whether the work is kept is the
    // caller's decision and has already been made by the time this runs.
    static void release(const std::string& transaction);

    // How many are open, and the most that may be. Abandoned transactions -- a
    // visitor who closes the tab halfway through -- are held until something
    // reclaims them, so there has to be a ceiling.
    static size_t count();
    static size_t limit();

    // Everything belonging to an owner, for when their session ends.
    static void release_owner(const std::string& owner);
  };

}

#endif // __RPCPOOL_HPP__
