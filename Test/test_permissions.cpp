// What a connection is allowed to reach.
//
// ZiguratIP keeps nothing about its users. A certificate says who its holder is
// and what that holder may do, and a file in the users directory says the
// subject is still welcome. There is no grant table, no account, nothing a
// restore could quietly bring back -- so these are the three pieces that have
// to hold: the matching rule, the name a subject is filed under, and the
// handshake that carries the whole lot from one end to the other.

#include "ztest.hpp"
#include "globals.hpp"
#include "utility.hpp"
#include "x509.hpp"
#include "tls.hpp"
#include "tlsbuf.hpp"
#include "tlsstream.hpp"
#include "filestream.hpp"
#include "bufferstream.hpp"
#include <thread>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <sys/socket.h>

using namespace Zigurat;


namespace
{
  // Globals::permits reads the peer bound to the calling thread, so a case that
  // sets one has to put it back: every other test in this binary runs on the
  // same thread and would inherit it.
  struct AsPeer
  {
    AsPeer(const std::vector<std::string>& permissions)
    {
      Globals::set_permissions_mode(true);
      Globals::set_peer("CN=under-test", permissions);
    }

    ~AsPeer()
    {
      Globals::clear_peer();
      Globals::set_permissions_mode(false);
    }
  };

  std::string sample_file(const std::string& name)
  {
    std::string found = Utility::config_path("cert/" + name);
    if (found.size() > 0) return found;

    const char* candidates[] = {"home/etc/cert/", "../home/etc/cert/"};
    for (int i = 0; i < 2; i++) {
      std::ifstream probe(std::string(candidates[i]) + name);
      if (probe.good()) return std::string(candidates[i]) + name;
    }
    return "";
  }
}


// ---------------------------------------------------------------------------
// The matching rule
// ---------------------------------------------------------------------------

// A permission is a path -- the schema levels, then the object -- and it covers
// what it names together with everything under it. That is what makes a single
// entry, "DEMO", worth issuing: it reaches the whole schema without listing it.
ZTEST(Permissions, a_schema_covers_every_object_in_it)
{
  AsPeer peer({"DEMO"});

  ZCHECK(Globals::permits("DEMO"));
  ZCHECK(Globals::permits("DEMO::AUTHORS"));
  ZCHECK(Globals::permits("DEMO::BOOKS"));
  ZCHECK(Globals::permits("DEMO::ADD_AUTHOR"));

  // A different schema, and a schema this one is merely a prefix of the text
  // of: matching is by level, not by characters.
  ZCHECK(!Globals::permits("BENCH::ITEM"));
  ZCHECK(!Globals::permits("DEMOGRAPHIC::CENSUS"));
}

// The other direction: an entry naming one object reaches that object and
// nothing beside it, which is what lets an issuer hand out a single table.
ZTEST(Permissions, an_object_covers_only_itself)
{
  AsPeer peer({"DEMO::AUTHORS"});

  ZCHECK(Globals::permits("DEMO::AUTHORS"));

  ZCHECK(!Globals::permits("DEMO"));
  ZCHECK(!Globals::permits("DEMO::BOOKS"));
  ZCHECK(!Globals::permits("DEMO::AUTHORS_SEQ"));
}

// Several entries on one certificate, which is the usual shape: a schema plus a
// table borrowed from somewhere else.
ZTEST(Permissions, entries_accumulate)
{
  AsPeer peer({"DEMO", "BENCH::ITEM"});

  ZCHECK(Globals::permits("DEMO::BOOKS"));
  ZCHECK(Globals::permits("BENCH::ITEM"));
  ZCHECK(!Globals::permits("BENCH::ORDER"));
}

// Parsi is case insensitive and upper cases what it compiles, so an issuer who
// writes the permission the way the source reads has to get the same answer.
ZTEST(Permissions, matching_ignores_case_and_surrounding_space)
{
  AsPeer peer({" demo ", "bench :: item"});

  ZCHECK(Globals::permits("DEMO::AUTHORS"));
  ZCHECK(Globals::permits("BENCH::ITEM"));
}

// A certificate carrying no permissions grants nothing. An issuer grants by
// naming: leaving the extension out has to mean nothing rather than everything,
// or every certificate issued before permissions existed would be a master key.
ZTEST(Permissions, a_certificate_without_permissions_reaches_nothing)
{
  AsPeer peer({});

  ZCHECK(!Globals::permits("DEMO"));
  ZCHECK(!Globals::permits("DEMO::AUTHORS"));
}

// One entry for an issuer who means it.
ZTEST(Permissions, a_wildcard_covers_everything)
{
  AsPeer peer({"*"});

  ZCHECK(Globals::permits("DEMO::AUTHORS"));
  ZCHECK(Globals::permits("ANYTHING::AT::ALL"));
}

// A plain connection has no peer to ask about, so nothing is enforced and the
// server behaves exactly as it did before any of this existed. Turning TLS on
// is what turns access control on.
ZTEST(Permissions, an_unidentified_connection_is_not_restricted)
{
  Globals::set_permissions_mode(true);
  Globals::clear_peer();

  ZCHECK(!Globals::identified());
  ZCHECK(Globals::permits("DEMO::AUTHORS"));
  ZCHECK(Globals::permits("ANYTHING"));

  Globals::set_permissions_mode(false);
}

// The switch itself. Off is the shipped default, and it has to mean off for
// everybody: the same peer that reaches nothing with it on reaches everything
// with it off, and the subject stays known either way so the log still says who
// did what.
ZTEST(Permissions, the_switch_turns_the_whole_thing_off)
{
  Globals::set_permissions_mode(false);
  Globals::set_peer("CN=under-test", {"DEMO"});

  ZCHECK(!Globals::permissions_mode());
  ZCHECK(Globals::permits("BENCH::ITEM"));
  ZCHECK_NOTHROW(Globals::require_permission("BENCH::ITEM"));
  ZCHECK_STR(Globals::peer_subject(), "CN=under-test");

  Globals::set_permissions_mode(true);
  ZCHECK(!Globals::permits("BENCH::ITEM"));
  ZCHECK_THROWS(Globals::require_permission("BENCH::ITEM"));

  Globals::clear_peer();
  Globals::set_permissions_mode(false);
}

// The refusing form, which is what the servers actually call. It has to name
// both the subject and what was asked for: an operator reading the log needs to
// know which certificate to reissue.
ZTEST(Permissions, refusing_says_who_was_refused_and_what_for)
{
  AsPeer peer({"DEMO"});

  ZCHECK_NOTHROW(Globals::require_permission("DEMO::AUTHORS"));

  std::string message;
  try {
    Globals::require_permission("BENCH::ITEM");
  } catch (const ZiguratException& refused) {
    message = refused.message();
  }

  ZCHECK(message.find("CN=under-test") != std::string::npos);
  ZCHECK(message.find("BENCH::ITEM") != std::string::npos);
}


// ---------------------------------------------------------------------------
// The name a subject is filed under
// ---------------------------------------------------------------------------

// The users directory is keyed on the distinguished name, and a distinguished
// name is not a file name: it carries commas, spaces, equals signs, and there is
// nothing to stop an issuer putting a slash or a pair of dots in one.
ZTEST(Permissions, a_subject_survives_being_written_down_as_a_file_name)
{
  const char* subjects[] = {
    "CN=alice",
    "C=US, O=Acme Widgets, CN=alice@example.com",
    "CN=../../etc/passwd",
    "CN=a/b/c",
    "CN=  spaced  out  ",
    "CN=\xC3\xA9lodie"
  };

  for (size_t i = 0; i < sizeof(subjects) / sizeof(subjects[0]); i++) {
    const std::string subject(subjects[i]);
    const std::string file_name = X509::subject_file_name(subject);

    ZCHECK_STR(X509::file_name_subject(file_name), subject);

    // And whatever it started as, it now names one entry in one directory.
    ZCHECK(file_name.find('/') == std::string::npos);
    ZCHECK(file_name.find("..") == std::string::npos);
    ZCHECK(!file_name.empty());
  }
}

// Two subjects that differ at all are filed separately, including when they
// differ only in the characters that had to be encoded.
ZTEST(Permissions, different_subjects_get_different_file_names)
{
  ZCHECK(X509::subject_file_name("CN=a b") != X509::subject_file_name("CN=a-b"));
  ZCHECK(X509::subject_file_name("CN=a,O=b") != X509::subject_file_name("CN=a, O=b"));

  // The common shape stays exactly as it reads, so the directory can be
  // understood at a glance. Only what a file name cannot hold gets encoded.
  ZCHECK_STR(X509::subject_file_name("CN=alice"), "CN=alice");
  ZCHECK_STR(X509::subject_file_name("C=US, CN=alice"), "C=US%2C%20CN=alice");
}


// ---------------------------------------------------------------------------
// Carrying it across a connection
// ---------------------------------------------------------------------------

namespace
{
  struct Pair
  {
    Socket::handle_t ends[2];
    bool ready;

    Pair() : ready(false)
    {
      int fds[2] = {-1, -1};
      ready = (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
      ends[0] = (Socket::handle_t)fds[0];
      ends[1] = (Socket::handle_t)fds[1];
    }
  };

  TLS::HandshakeParameters sample_parameters()
  {
    TLS::HandshakeParameters params;
    params.protocol_version = TLS::VERSION_1_2;
    params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_256_CBC_SHA256);
    params.compression_methods.push_back(TLS::CompressionMethod::NONE);

    params.credentials.certificate = sample_file("dont-use-certificate.crt");
    params.credentials.private_key = sample_file("dont-use-private.key");
    params.credentials.authority   = sample_file("dont-use-certificate.crt");
    return params;
  }

  // A certificate the sample authority signed, carrying the permissions given.
  // It is issued against the sample key pair, so the holder's private key is
  // the sample private key -- which is exactly what makes this cheap enough to
  // do inside a test: no key generation, and the authority still vouches for it.
  std::string issue_holder(const std::vector<std::string>& granted, const std::string& common_name)
  {
    const std::string issuer_path = sample_file("issuer.conf");
    const std::string key_path    = sample_file("dont-use-private.key");
    if (issuer_path.empty() || key_path.empty()) return "";

    const std::string subject_conf_text = "COUNTRY: US\nCOMMON_NAME: " + common_name + "\n";
    bufferstream subject_conf;
    subject_conf.write(subject_conf_text.data(), (std::streamsize)subject_conf_text.size());

    filestream subject_key(key_path, std::ios::in | std::ios::binary);
    bufferstream csr;
    X509::csr(subject_conf, subject_key, "", "SHA-256", "DER", csr);

    filestream issuer(issuer_path, std::ios::in | std::ios::binary);
    filestream issuer_key(key_path, std::ios::in | std::ios::binary);
    bufferstream serial, certificate;
    serial.write_std_ubyte(0x53);

    X509::issue(serial, issuer, issuer_key, "", std::time(0), std::time(0) + 3600,
		csr, "SHA-256", "DER", granted, certificate);

    const std::string path = std::string("holder-") + common_name + ".crt";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const std::string content = certificate.string();
    out.write(content.data(), (std::streamsize)content.size());
    out.close();

    return path;
  }
}

// The whole arrangement in one exchange: the client presents a certificate the
// authority issued, the server reads out of it both who the client is and what
// it may do, and neither answer came from anything the server had stored.
ZTEST(Permissions, the_handshake_carries_what_the_certificate_grants)
{
  TLS::HandshakeParameters server_params = sample_parameters();
  if (server_params.credentials.certificate.empty()) { ZCHECK(false); return; }

  const std::string holder = issue_holder({"DEMO", "BENCH::ITEM"}, "permitted-client");
  if (holder.empty()) { ZCHECK(false); return; }

  TLS::HandshakeParameters client_params = sample_parameters();
  client_params.credentials.certificate = holder;

  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  std::string server_error, server_peer;
  std::vector<std::string> server_saw;

  std::thread listener([&] () {
      try {
	tlsstream server;
	server.open(TLS::ConnectionEnd::SERVER, server_params, pair.ends[1], true, 0);
	server_peer = server.peer_subject();
	server_saw = server.peer_permissions();
      } catch (const std::exception& error) {
	server_error = error.what();
      } catch (...) {
	server_error = "unknown";
      }
    });

  std::string client_error;
  try {
    tlsstream client;
    client.open(TLS::ConnectionEnd::CLIENT, client_params, pair.ends[0], true, 0);
  } catch (const std::exception& error) {
    client_error = error.what();
  } catch (...) {
    client_error = "unknown";
  }

  listener.join();
  Utility::remove_file(holder);

  ZCHECK_STR(server_error, "");
  ZCHECK_STR(client_error, "");

  ZCHECK(server_peer.find("CN=permitted-client") != std::string::npos);

  ZCHECK_EQ(server_saw.size(), (size_t)2);
  if (server_saw.size() == 2) {
    ZCHECK_STR(server_saw[0], "DEMO");
    ZCHECK_STR(server_saw[1], "BENCH::ITEM");
  }

  // And those permissions, bound to a thread, decide what it may reach.
  Globals::set_permissions_mode(true);
  Globals::set_peer(server_peer, server_saw);
  ZCHECK(Globals::permits("DEMO::AUTHORS"));
  ZCHECK(Globals::permits("BENCH::ITEM"));
  ZCHECK(!Globals::permits("BENCH::ORDER"));
  Globals::clear_peer();
  Globals::set_permissions_mode(false);
}

// Being genuine is not the same as being welcome. The users directory is asked
// after the certificate checks out and before the handshake finishes, so a
// subject that has been taken off it cannot get as far as sending a request --
// whichever of its certificates it holds.
ZTEST(Permissions, an_unregistered_subject_is_refused_during_the_handshake)
{
  TLS::HandshakeParameters server_params = sample_parameters();
  if (server_params.credentials.certificate.empty()) { ZCHECK(false); return; }

  std::string asked_about;
  server_params.authorize = [&] (const std::string& subject) -> bool {
    asked_about = subject;
    return false;                                  // taken off the register
  };

  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  std::string server_error;
  std::thread listener([&] () {
      try {
	tlsstream server;
	server.open(TLS::ConnectionEnd::SERVER, server_params, pair.ends[1], true, 0);
	server_error = "";                          // completed, which it must not
      } catch (const std::exception& error) {
	server_error = error.what();
      } catch (...) {
	server_error = "unknown";
      }
    });

  std::string client_error;
  try {
    tlsstream client;
    client.open(TLS::ConnectionEnd::CLIENT, sample_parameters(), pair.ends[0], true, 0);
    client_error = "";
  } catch (const std::exception& error) {
    client_error = error.what();
  } catch (...) {
    client_error = "unknown";
  }

  listener.join();

  // It was asked, and it was asked about the right peer.
  ZCHECK(asked_about.find("CN=ZiguratIP") != std::string::npos);

  // The server refused, and the client was told rather than left waiting.
  ZCHECK(!server_error.empty());
  ZCHECK(!client_error.empty());
}

// The registered case, through the same hook, so the refusal above is the
// policy answering and not the policy merely being present.
ZTEST(Permissions, a_registered_subject_is_let_through)
{
  TLS::HandshakeParameters server_params = sample_parameters();
  if (server_params.credentials.certificate.empty()) { ZCHECK(false); return; }

  server_params.authorize = [] (const std::string&) -> bool { return true; };

  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  std::string server_error;
  std::thread listener([&] () {
      try {
	tlsstream server;
	server.open(TLS::ConnectionEnd::SERVER, server_params, pair.ends[1], true, 0);
      } catch (const std::exception& error) {
	server_error = error.what();
      } catch (...) {
	server_error = "unknown";
      }
    });

  std::string client_error;
  try {
    tlsstream client;
    client.open(TLS::ConnectionEnd::CLIENT, sample_parameters(), pair.ends[0], true, 0);
  } catch (const std::exception& error) {
    client_error = error.what();
  } catch (...) {
    client_error = "unknown";
  }

  listener.join();

  ZCHECK_STR(server_error, "");
  ZCHECK_STR(client_error, "");
}
