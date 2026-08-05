#include "rpcpool.hpp"
#include "zexception.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <string>


namespace Zigurat
{

  namespace
  {
    struct Entry
    {
      std::unique_ptr<Connector> connection;
      std::string owner;
    };

    std::map<std::string, Entry>& entries()
    {
      static std::map<std::string, Entry> held;
      return held;
    }

    std::mutex& guard()
    {
      static std::mutex lock;
      return lock;
    }

    const size_t LIMIT = 256;
  }

  std::string RPCPool::owner(const std::string& peer_subject, const std::string& session_id)
  {
    // A certificate subject is an authenticated identity: the peer proved it
    // holds the key, against an authority this server trusts, and it is checked
    // again on every request. Prefer it -- the same person is then the same
    // owner however many browsers they use.
    if (!peer_subject.empty()) return "peer:" + peer_subject;

    // Otherwise there is nobody to be sure about, and the cookie is the best
    // handle there is. It names a browser rather than a person: whoever holds
    // the cookie is that session. Worth being plain about, because this is what
    // owns a transaction whenever TLS_CLIENT_AUTH is NONE.
    //
    // Prefixed, so a certificate subject cannot be crafted to collide with a
    // session id and inherit its transactions.
    if (!session_id.empty()) return "session:" + session_id;

    throw ZiguratException(7807, "no identity for this request: over mutual TLS the client"
			   " certificate names the owner, otherwise call"
			   " Session::INITIALIZE(request, response) first");
  }

  std::string RPCPool::open(const std::string& owner)
  {
    if (owner.empty()) throw ZiguratException(7807, "cannot open a transaction for nobody");

    std::unique_ptr<Connector> connection(new Connector());
    connection->open();

    const std::string transaction = std::to_string(connection->transaction_id());

    std::lock_guard<std::mutex> lock(guard());

    if (entries().size() >= LIMIT) {
      connection->close();
      throw ZiguratException(7808, "too many open transactions; commit or roll back the ones you hold");
    }

    // A transaction id already names one of these. That is the storage engine
    // handing out an id twice, which nothing here can paper over.
    if (entries().find(transaction) != entries().end()) {
      connection->close();
      throw ZiguratException(7808, "transaction " + transaction + " is already open");
    }

    Entry entry;
    entry.connection = std::move(connection);
    entry.owner = owner;
    entries()[transaction] = std::move(entry);

    return transaction;
  }

  Connector& RPCPool::held(const std::string& transaction, const std::string& owner)
  {
    if (transaction.empty()) throw ZiguratException(7809, "no transaction named");

    std::lock_guard<std::mutex> lock(guard());

    std::map<std::string, Entry>::iterator found = entries().find(transaction);
    if (found == entries().end())
      throw ZiguratException(7809, "no such transaction: it was committed, rolled back, or never opened");

    // Named correctly and owned by somebody else. The message deliberately does
    // not say whose -- that a transaction exists at all is already more than a
    // stranger needs to know.
    if (found->second.owner != owner)
      throw ZiguratException(7809, "that transaction belongs to another session");

    return *found->second.connection;
  }

  void RPCPool::release(const std::string& transaction)
  {
    std::lock_guard<std::mutex> lock(guard());

    std::map<std::string, Entry>::iterator found = entries().find(transaction);
    if (found == entries().end()) return;   // releasing twice is not an error

    try {
      found->second.connection->close();
    } catch (...) {
      // A peer that has already gone is not a reason to keep the entry.
    }
    entries().erase(found);
  }

  void RPCPool::release_owner(const std::string& owner)
  {
    std::lock_guard<std::mutex> lock(guard());

    std::map<std::string, Entry>::iterator it = entries().begin();
    while (it != entries().end()) {
      if (it->second.owner == owner) {
	try { it->second.connection->close(); } catch (...) { }
	it = entries().erase(it);
      } else {
	++it;
      }
    }
  }

  size_t RPCPool::count()
  {
    std::lock_guard<std::mutex> lock(guard());
    return entries().size();
  }

  size_t RPCPool::limit()
  {
    return LIMIT;
  }

}
