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

  // Reaches tlsbuf's protected record layer, and stands in for the handshake by
  // installing an agreed cipher state directly.
  class Endpoint : public tlsbuf
  {
  public:
    void attach(TLS::ConnectionEnd entity, const TLS::CipherSuite& suite,
		Socket::handle_t handle, const uint8_t* master_secret)
    {
      this->_tcpstream.open(handle, true, 0);

      std::memset(&this->_write_state, 0x00, sizeof(TLS::SecurityParameters));
      this->_write_state.entity = entity;
      this->_write_state.prf_algorithm = TLS::PRFAlgorithm::TLS_PRF_SHA256;
      this->_write_state.compression_algorithm = TLS::CompressionMethod::NONE;
      TLS::cipher_suite(suite, this->_write_state);

      std::memcpy(this->_write_state.master_secret, master_secret, TLS::MASTER_SECRET_LENGTH);
      std::memset(this->_write_state.client_random, 0xA5, TLS::RANDOM_LENGTH);
      std::memset(this->_write_state.server_random, 0x5A, TLS::RANDOM_LENGTH);

      // Both directions are settled here, which is the state a finished
      // handshake leaves them in.
      this->_read_state = this->_write_state;

      const TLS::SecurityParameters& p = this->_write_state;
      this->client_write_MAC_key = new uint8_t[p.mac_key_length ? p.mac_key_length : 1];
      this->server_write_MAC_key = new uint8_t[p.mac_key_length ? p.mac_key_length : 1];
      this->client_write_key     = new uint8_t[p.enc_key_length ? p.enc_key_length : 1];
      this->server_write_key     = new uint8_t[p.enc_key_length ? p.enc_key_length : 1];
      this->client_write_IV      = new uint8_t[p.fixed_iv_length ? p.fixed_iv_length : 1];
      this->server_write_IV      = new uint8_t[p.fixed_iv_length ? p.fixed_iv_length : 1];

      TLS::calculate_keys(this->_write_state,
			  this->client_write_MAC_key, this->server_write_MAC_key,
			  this->client_write_key,     this->server_write_key,
			  this->client_write_IV,      this->server_write_IV);
    }

    void put(TLS::ContentType type, const std::string& text)
    {
      bufferstream fragment;
      fragment.write(text.data(), (std::streamsize)text.size());
      TLS::Record record {type, TLS::VERSION_1_2, (std::streamsize)text.size(), fragment};
      this->_send_record(record);
    }

    std::string get()
    {
      bufferstream fragment;
      TLS::Record record {(TLS::ContentType)0, {0, 0}, 0, fragment};
      this->_recv_record(record);

      std::string text((size_t)record.length, '\0');
      if (record.length > 0)
	fragment.read(&text[0], 0, record.length);
      return text;
    }

    void detach() { this->_tcpstream.close(); }
  };

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

  const uint8_t* sample_master_secret()
  {
    static uint8_t secret[TLS::MASTER_SECRET_LENGTH];
    for (size_t i = 0; i < TLS::MASTER_SECRET_LENGTH; i++) secret[i] = (uint8_t)(i * 7 + 1);
    return secret;
  }

  // Every suite the record layer claims to implement, so the ones that differ in
  // block size, MAC length and whether they encrypt at all are all covered.
  struct Suite
  {
    const char*             name;
    const TLS::CipherSuite* suite;
  };

  const Suite SUITES[] = {
    {"NULL_WITH_NULL_NULL",       &TLS::TLS_NULL_WITH_NULL_NULL},
    {"RSA_WITH_NULL_SHA",         &TLS::TLS_RSA_WITH_NULL_SHA},
    {"RSA_WITH_NULL_SHA256",      &TLS::TLS_RSA_WITH_NULL_SHA256},
    {"RSA_WITH_AES_128_CBC_SHA",  &TLS::TLS_RSA_WITH_AES_128_CBC_SHA},
    {"RSA_WITH_AES_256_CBC_SHA",  &TLS::TLS_RSA_WITH_AES_256_CBC_SHA},
    {"RSA_WITH_AES_128_CBC_SHA256", &TLS::TLS_RSA_WITH_AES_128_CBC_SHA256},
    {"RSA_WITH_AES_256_CBC_SHA256", &TLS::TLS_RSA_WITH_AES_256_CBC_SHA256}
  };

  const int SUITE_COUNT = (int)(sizeof(SUITES) / sizeof(SUITES[0]));

}


// A record written by one end is the record the other end reads, under every
// suite. The CBC ones are the interesting case: the plain text has to survive
// the explicit IV, the MAC and the padding that the block cipher needs.
ZTEST(TLS, a_record_survives_every_cipher_suite)
{
  for (int i = 0; i < SUITE_COUNT; i++) {
    Pair pair;
    if (!pair.ready) { ZCHECK(false); return; }

    Endpoint client, server;
    client.attach(TLS::ConnectionEnd::CLIENT, *SUITES[i].suite, pair.ends[0], sample_master_secret());
    server.attach(TLS::ConnectionEnd::SERVER, *SUITES[i].suite, pair.ends[1], sample_master_secret());

    const std::string sent = "the quick brown fox jumps over the lazy dog";
    client.put(TLS::ContentType::APPLICATION_DATA, sent);
    ZCHECK_STR(server.get(), sent);

    const std::string replied = "and back again";
    server.put(TLS::ContentType::APPLICATION_DATA, replied);
    ZCHECK_STR(client.get(), replied);

    client.detach();
    server.detach();
  }
}

// Padding is the part that was wrong: it was computed as the remainder rather
// than the distance to the next block boundary. A length one short of a block,
// exactly on one, and one past it are where that shows.
ZTEST(TLS, cbc_records_pad_to_the_block_boundary_at_every_length)
{
  for (size_t length = 0; length <= 64; length++) {
    Pair pair;
    if (!pair.ready) { ZCHECK(false); return; }

    Endpoint client, server;
    client.attach(TLS::ConnectionEnd::CLIENT, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		  pair.ends[0], sample_master_secret());
    server.attach(TLS::ConnectionEnd::SERVER, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		  pair.ends[1], sample_master_secret());

    const std::string sent(length, (char)('a' + (length % 26)));
    client.put(TLS::ContentType::APPLICATION_DATA, sent);
    ZCHECK_STR(server.get(), sent);

    client.detach();
    server.detach();
  }
}

// The MAC covers a sequence number that each end counts for itself. A single
// shared counter drifted the moment traffic stopped alternating, so a run of
// records in one direction is what catches it.
ZTEST(TLS, sequence_numbers_are_counted_per_direction)
{
  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  Endpoint client, server;
  client.attach(TLS::ConnectionEnd::CLIENT, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		pair.ends[0], sample_master_secret());
  server.attach(TLS::ConnectionEnd::SERVER, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		pair.ends[1], sample_master_secret());

  // Five in one direction before anything comes back.
  for (int i = 0; i < 5; i++)
    client.put(TLS::ContentType::APPLICATION_DATA, "client says " + std::to_string(i));
  for (int i = 0; i < 5; i++)
    ZCHECK_STR(server.get(), "client says " + std::to_string(i));

  // Then the other way, and then interleaved.
  for (int i = 0; i < 3; i++)
    server.put(TLS::ContentType::APPLICATION_DATA, "server says " + std::to_string(i));
  for (int i = 0; i < 3; i++)
    ZCHECK_STR(client.get(), "server says " + std::to_string(i));

  for (int i = 0; i < 4; i++) {
    client.put(TLS::ContentType::APPLICATION_DATA, "ping " + std::to_string(i));
    ZCHECK_STR(server.get(), "ping " + std::to_string(i));
    server.put(TLS::ContentType::APPLICATION_DATA, "pong " + std::to_string(i));
    ZCHECK_STR(client.get(), "pong " + std::to_string(i));
  }

  client.detach();
  server.detach();
}

// Records larger than the 2048 octet transport buffer, including the exact
// multiples of it. Those were the lengths where TLS::MAC asked for htons(length)
// octets of fragment -- eight of them, for 2048 -- and ran the rest of the MAC
// over uninitialised stack.
ZTEST(TLS, a_large_fragment_survives)
{
  // Capped below the socket pair's own buffer: both ends live in one thread
  // here, so a record larger than the kernel will hold has nobody to drain it.
  const size_t sizes[] = {1000, 2040, 2048, 3000, 4096, 5000};

  for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
    Pair pair;
    if (!pair.ready) { ZCHECK(false); return; }

    Endpoint client, server;
    client.attach(TLS::ConnectionEnd::CLIENT, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		  pair.ends[0], sample_master_secret());
    server.attach(TLS::ConnectionEnd::SERVER, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		  pair.ends[1], sample_master_secret());

    std::string sent;
    sent.reserve(sizes[s]);
    for (size_t i = 0; i < sizes[s]; i++) sent += (char)(i % 251);

    client.put(TLS::ContentType::APPLICATION_DATA, sent);
    ZCHECK_STR(server.get(), sent);

    client.detach();
    server.detach();
  }
}

// Closing is a clean shutdown, not a failure. close_notify used to go out at
// FATAL, which made _alert throw on the way out of every healthy close. Closing
// the second end, once its peer has already gone, has to come back too.
ZTEST(TLS, closing_a_connection_does_not_throw)
{
  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  Endpoint client, server;
  client.attach(TLS::ConnectionEnd::CLIENT, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		pair.ends[0], sample_master_secret());
  server.attach(TLS::ConnectionEnd::SERVER, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		pair.ends[1], sample_master_secret());

  ZCHECK(client.is_open());
  ZCHECK_NOTHROW(client.close());
  ZCHECK(!client.is_open());

  ZCHECK_NOTHROW(server.close());
  ZCHECK(!server.is_open());
}

// Writing to a socket whose peer has hung up must fail, not kill the process.
// SIGPIPE was only ignored inside the server's own main, so the connector, this
// test binary and anybody's client died the moment the far end went away. The
// socket layer suppresses it now, per socket or per send depending on platform.
ZTEST(TLS, writing_to_a_departed_peer_does_not_kill_the_process)
{
  Pair pair;
  if (!pair.ready) { ZCHECK(false); return; }

  Endpoint client, server;
  client.attach(TLS::ConnectionEnd::CLIENT, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		pair.ends[0], sample_master_secret());
  server.attach(TLS::ConnectionEnd::SERVER, TLS::TLS_RSA_WITH_AES_256_CBC_SHA256,
		pair.ends[1], sample_master_secret());

  client.detach();   // the peer simply goes

  // Reaching this line at all is most of the point.
  ZCHECK_NOTHROW(server.put(TLS::ContentType::APPLICATION_DATA, "into the void"));
  server.detach();
}

// Record IVs come from the platform's entropy. They were derived from
// srand(time(nullptr)), so every IV within a second was the same octets.
ZTEST(TLS, record_ivs_do_not_repeat)
{
  const int count = 64;
  std::string seen[count];

  for (int i = 0; i < count; i++) {
    uint8_t iv[16];
    TLS::IV(iv, sizeof(iv));
    seen[i].assign((const char*)iv, sizeof(iv));
  }

  int duplicates = 0;
  for (int i = 0; i < count; i++)
    for (int j = i + 1; j < count; j++)
      if (seen[i] == seen[j]) duplicates++;

  ZCHECK_EQ(duplicates, 0);

  // And an IV is not all one value, which a zeroed or unwritten buffer would be.
  bool varied = false;
  for (size_t k = 1; k < seen[0].size(); k++)
    if (seen[0][k] != seen[0][0]) varied = true;
  ZCHECK(varied);
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
  ZCHECK(server_error.find("tbsCertificate") != std::string::npos);

  // And the client is told, rather than being left waiting.
  ZCHECK(!client_error.empty());
}

// The TLS 1.2 pseudo random function, against the vector circulated on the IETF
// TLS list. Everything a connection is protected with comes out of this -- the
// master secret, then every key and MAC key -- so an implementation that agrees
// only with itself protects nothing that anybody else can check.
ZTEST(TLS, the_prf_matches_the_specification)
{
  const uint8_t secret[] = {0x9b,0xbe,0x43,0x6b,0xa9,0x40,0xf0,0x17,0xb1,0x76,0x52,0x84,0x9a,0x71,0xdb,0x35};
  const uint8_t seed[]   = {0xa0,0xba,0x9f,0x93,0x6c,0xda,0x31,0x18,0x27,0xa6,0xf7,0x96,0xff,0xd5,0x19,0x8c};
  const char*   label    = "test label";

  const uint8_t want[] = {
    0xe3,0xf2,0x29,0xba,0x72,0x7b,0xe1,0x7b,0x8d,0x12,0x26,0x20,0x55,0x7c,0xd4,0x53,
    0xc2,0xaa,0xb2,0x1d,0x07,0xc3,0xd4,0x95,0x32,0x9b,0x52,0xd4,0xe6,0x1e,0xdb,0x5a,
    0x6b,0x30,0x17,0x91,0xe9,0x0d,0x35,0xc9,0xc9,0xa4,0x6b,0x4e,0x14,0xba,0xf9,0xaf,
    0x0f,0xa0,0x22,0xf7,0x07,0x7d,0xef,0x17,0xab,0xfd,0x37,0x97,0xc0,0x56,0x4b,0xab,
    0x4f,0xbc,0x91,0x66,0x6e,0x9d,0xef,0x9b,0x97,0xfc,0xe3,0x4f,0x79,0x67,0x89,0xba,
    0xa4,0x80,0x82,0xd1,0x22,0xee,0x42,0xc5,0xa7,0x2e,0x5a,0x51,0x10,0xff,0xf7,0x01,
    0x87,0x34,0x7b,0x66};

  uint8_t got[sizeof(want)];
  TLS::PRF(TLS::PRFAlgorithm::TLS_PRF_SHA256,
	   secret, sizeof(secret),
	   (const uint8_t*)label, 10,
	   seed, sizeof(seed),
	   got, sizeof(got));

  ZCHECK(std::memcmp(got, want, sizeof(want)) == 0);
}
