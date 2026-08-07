// The TLS record layer.
//
// tlsbuf's record layer is what every handshake message and every byte of
// application data travels inside, so it is worth holding to account on its own,
// before any handshake exists to exercise it. These drive two tlsbufs across a
// socket pair with keys agreed out of band, which is the state the handshake
// leaves them in.

#include "ztest.hpp"
#include "tlsbuf.hpp"
#include "tls.hpp"
#include "tlsexception.hpp"
#include "bufferstream.hpp"
#include "tlsstream.hpp"
#include "utility.hpp"
#include <thread>
#include <fstream>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace Zigurat;


namespace
{

  // The record layer this used to reach into is gone: an Endpoint that
  // installed an agreed cipher state directly has nothing to install it into
  // now, and the suites it swept are OpenSSL's business rather than this
  // repository's. What is left below tests the seam -- a streambuf over a
  // connection -- which is the part this project still owns.

  // A connected pair, so both ends live in one process and one thread. Records
  // are small and the socket buffers hold them, so nothing blocks.
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

}










// ---------------------------------------------------------------------------
// The handshake
// ---------------------------------------------------------------------------

namespace
{
  std::string cert_file(const std::string& name)
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

  // The shipped sample is self signed, so it is its own authority: an end can
  // present it and it validates against itself. That exercises every step of the
  // exchange without spending half a minute generating key pairs.
  TLS::HandshakeParameters sample_parameters()
  {
    TLS::HandshakeParameters params;
    params.protocol_version = TLS::VERSION_1_2;
    params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_256_CBC_SHA256);
    params.compression_methods.push_back(TLS::CompressionMethod::NONE);

    params.credentials.certificate = cert_file("dont-use-certificate.crt");
    params.credentials.private_key = cert_file("dont-use-private.key");
    params.credentials.authority   = cert_file("dont-use-certificate.crt");
    return params;
  }
}

// Both ends prove themselves, agree a secret neither sent, and then talk. The
// server is driven on its own thread because a handshake is a conversation --
// each end blocks waiting for the other.
ZTEST(TLS, a_handshake_authenticates_both_ends_and_carries_data)
{
  TLS::HandshakeParameters params = sample_parameters();
  if (params.credentials.certificate.empty()) { ZCHECK(false); return; }

  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  std::string server_error, server_saw, server_peer;

  std::thread listener([&] () {
      try {
	tlsstream server;
	server.open(TLS::ConnectionEnd::SERVER, params, pair.ends[1], true, 0);

	std::string line;
	std::getline(server, line);
	server_saw = line;
	server_peer = dynamic_cast<tlsbuf*>(server.rdbuf())->peer_subject();

	server << "and to you" << std::endl;
	server.flush();
      } catch (const std::exception& error) {
	server_error = error.what();
      } catch (...) {
	server_error = "unknown";
      }
    });

  std::string client_error, client_saw, client_peer;
  try {
    tlsstream client;
    client.open(TLS::ConnectionEnd::CLIENT, params, pair.ends[0], true, 0);

    client_peer = dynamic_cast<tlsbuf*>(client.rdbuf())->peer_subject();

    client << "good day" << std::endl;
    client.flush();

    std::string line;
    std::getline(client, line);
    client_saw = line;
  } catch (const std::exception& error) {
    client_error = error.what();
  } catch (...) {
    client_error = "unknown";
  }

  listener.join();

  ZCHECK_STR(server_error, "");
  ZCHECK_STR(client_error, "");

  // Each end learned who the other is, from the certificate it presented.
  ZCHECK(server_peer.find("CN=ZiguratIP") != std::string::npos);
  ZCHECK(client_peer.find("CN=ZiguratIP") != std::string::npos);

  // And the data went through the encrypted records.
  ZCHECK_STR(server_saw, "good day");
  ZCHECK_STR(client_saw, "and to you");
}

// The whole point of the arrangement: an end that cannot vouch for its peer does
// not let it in, and both sides come apart cleanly when that happens rather than
// hanging or dying. The authority here is a public key file, which is a well
// formed DER object and not a certificate -- reading a tbsCertificate out of it
// used to walk past the buffer and take the process down with a bus error.
ZTEST(TLS, a_peer_that_cannot_be_vouched_for_is_refused)
{
  TLS::HandshakeParameters client_params = sample_parameters();
  if (client_params.credentials.certificate.empty()) { ZCHECK(false); return; }

  TLS::HandshakeParameters server_params = sample_parameters();
  server_params.credentials.authority = cert_file("dont-use-public.key");

  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  std::string server_error;
  std::thread listener([&] () {
      try {
	tlsstream server;
	server.open(TLS::ConnectionEnd::SERVER, server_params, pair.ends[1], true, 0);
	server_error = "";                       // completed, which it must not
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
    client_error = "";
  } catch (const std::exception& error) {
    client_error = error.what();
  } catch (...) {
    client_error = "unknown";
  }

  listener.join();   // reaching this at all is half the point

  // The server is the end that judges, so it is the end that must refuse, and
  // it must say why rather than falling over.
  ZCHECK(!server_error.empty());
  // It named "tbsCertificate" when this was walked by hand and the walk ran
  // out inside that field. The reader is OpenSSL's now and says the thing the
  // comment above says -- the authority is not a certificate -- so what is
  // checked here is that the refusal is about the authority, which is tlsbuf's
  // own words, rather than a field name belonging to a decoder that is gone.
  ZCHECK(server_error.find("certificate authority") != std::string::npos);

  // And the client is told, rather than being left waiting.
  ZCHECK(!client_error.empty());
}

// The seam tests, rebuilt on a real handshake. They used to install a cipher
// state by hand and drive the record layer; there is no record layer here to
// drive, so they do what a caller does -- connect, and use the stream.
namespace
{
  // Runs a server on its own thread and gives the caller the client end.
  // A handshake is a conversation, so both ends have to be live at once.
  struct Connected
  {
    Pair pair;
    tlsstream client, server;
    std::thread listener;
    std::string server_error;
    bool ready = false;

    Connected()
    {
      TLS::HandshakeParameters params = sample_parameters();
      if (params.credentials.certificate.empty() || !pair.ready) return;

      listener = std::thread([&] () {
	  try { server.open(TLS::ConnectionEnd::SERVER, params, pair.ends[1], true, 0); }
	  catch (const std::exception& error) { server_error = error.what(); }
	});
      try { client.open(TLS::ConnectionEnd::CLIENT, params, pair.ends[0], true, 0); }
      catch (const std::exception&) { }
      listener.join();
      ready = server_error.empty();
    }
  };
}

// More than the sixteen kilobyte buffer, and the exact multiple of it, sent
// through the stream rather than handed to a record.
ZTEST(TLS, a_large_payload_survives_the_stream)
{
  Connected link;
  if (!link.ready) { ZCHECK(false); return; }

  const size_t sizes[] = {1000, 4096, 16384, 20000};
  for (size_t size : sizes) {
    const std::string sent(size, 'z');

    std::string got;
    std::thread reader([&] () {
	got.resize(size);
	link.server.read(&got[0], (std::streamsize)size);
	got.resize((size_t)link.server.gcount());
      });

    link.client.write(sent.data(), (std::streamsize)size);
    link.client.flush();
    reader.join();

    ZCHECK_EQ(got.size(), size);
    ZCHECK(got == sent);
  }
}

// Closing a healthy connection is not an error. close_notify used to be sent
// at FATAL, which made the alert throw on the way out of every clean shutdown.
ZTEST(TLS, closing_a_connection_does_not_throw)
{
  Connected link;
  if (!link.ready) { ZCHECK(false); return; }

  ZCHECK_NOTHROW(link.client.close());
  ZCHECK_NOTHROW(link.server.close());
}

// A peer that has gone is not a reason to take the process down with it.
ZTEST(TLS, writing_to_a_departed_peer_does_not_kill_the_process)
{
  Connected link;
  if (!link.ready) { ZCHECK(false); return; }

  link.server.close();

  ZCHECK_NOTHROW({
      for (int i = 0; i < 64; i++) {
	link.client.write("still here", 10);
	link.client.flush();
      }
    });
}
