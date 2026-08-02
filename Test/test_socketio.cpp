#include "ztest.hpp"
#include "socket.hpp"
#include "socketbuf.hpp"
#include "tcpbuf.hpp"
#include "tcpstream.hpp"
#include "tcpserver.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <cstring>
#include <sys/socket.h>

using namespace Zigurat;


namespace
{
  // A socketpair is a real, connected, full duplex socket without a port, a
  // listener or a race, which keeps these cases deterministic.
  struct Pair
  {
    Socket::handle_t fds[2];
    bool ok;

    Pair() : ok(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0)
    {
      if (!ok) { fds[0] = Socket::INVALID_SOCKET; fds[1] = Socket::INVALID_SOCKET; }
    }

    ~Pair()
    {
      if (fds[0] != Socket::INVALID_SOCKET) ::close(fds[0]);
      if (fds[1] != Socket::INVALID_SOCKET) ::close(fds[1]);
    }
  };
}


ZTEST(SocketIO, socketpair_available)
{
  Pair pair;
  ZCHECK(pair.ok);
}

ZTEST(SocketIO, tcpstream_short_roundtrip)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);
  ZCHECK(a.is_open());
  ZCHECK(b.is_open());

  a.write("ZiguratIP", 9);
  a.flush();
  ZCHECK(a.good());

  char out[10];
  std::memset(out, 0, sizeof(out));
  b.read(out, 9);
  ZCHECK_EQ((long)b.gcount(), 9L);
  ZCHECK_STR(std::string(out, 9), "ZiguratIP");

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;  // the streams own them now
}

// The regression that matters most: the get and the put area used to be the
// same memory, so filling the receive buffer destroyed unflushed output.
ZTEST(SocketIO, receiving_does_not_clobber_pending_output)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);

  // Parked in a's put area, deliberately not flushed.
  a.write("HELLO", 5);

  b.write("XYZ", 3);
  b.flush();

  // Reading pulls a whole datagram into a's get area.
  char probe[4];
  std::memset(probe, 0, sizeof(probe));
  a.read(probe, 3);
  ZCHECK_STR(std::string(probe, 3), "XYZ");

  // The parked output must have survived the receive.
  a.flush();
  ZCHECK(a.good());

  char out[6];
  std::memset(out, 0, sizeof(out));
  b.read(out, 5);
  ZCHECK_EQ((long)b.gcount(), 5L);
  ZCHECK_STR(std::string(out, 5), "HELLO");

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;
}

// sync() used to call underflow(), so flushing blocked until the peer happened
// to send something. Flushing with nothing inbound must return straight away.
ZTEST(SocketIO, flush_does_not_wait_for_inbound_data)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);

  std::atomic<bool> done(false);
  std::thread worker([&a, &done] () {
      a.write("no reply expected", 17);
      a.flush();
      done = true;
    });

  for (int i = 0; i < 200 && !done; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  ZCHECK(done.load());
  worker.join();

  if (done) {
    char out[18];
    std::memset(out, 0, sizeof(out));
    b.read(out, 17);
    ZCHECK_STR(std::string(out, 17), "no reply expected");
  }

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;
}

// Well past the 2048 byte buffer, so overflow() and underflow() both cycle many
// times and any short send has to be resumed rather than silently dropped.
ZTEST(SocketIO, large_payload_survives_many_buffer_cycles)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  const size_t SIZE = 200000;
  std::string payload;
  payload.reserve(SIZE);
  for (size_t i = 0; i < SIZE; i++) payload.push_back((char)('A' + (i % 26)));

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);

  std::thread sender([&a, &payload] () {
      a.write(payload.c_str(), (std::streamsize)payload.size());
      a.flush();
      a.close();
    });

  std::string received;
  received.resize(SIZE);
  b.read(&received[0], (std::streamsize)SIZE);
  const long got = (long)b.gcount();

  sender.join();

  ZCHECK_EQ(got, (long)SIZE);
  if (got == (long)SIZE) ZCHECK(received == payload);

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;
}

ZTEST(SocketIO, typed_roundtrip_over_a_socket)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);

  a.write_std_ubyte((uint8_t)7);
  a.write_std_int((int32_t)-99);
  a.write_std_ulong((uint64_t)0x1122334455667788ull);
  a.write_std_string(std::string("handshake"));
  a.flush();

  ZCHECK_EQ((int)b.read_std_ubyte(), 7);
  ZCHECK_EQ(b.read_std_int(), (int32_t)-99);
  ZCHECK_EQ(b.read_std_ulong(), (uint64_t)0x1122334455667788ull);
  ZCHECK_STR(b.read_std_string(), "handshake");

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;
}

ZTEST(SocketIO, closed_peer_reports_eof)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);

  a.write("bye", 3);
  a.flush();
  a.close();

  char out[4];
  std::memset(out, 0, sizeof(out));
  b.read(out, 3);
  ZCHECK_STR(std::string(out, 3), "bye");

  // Nothing more will ever arrive.
  ZCHECK(b.get() == std::char_traits<char>::eof());
  ZCHECK(b.eof());

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;
}

ZTEST(SocketIO, tcpstream_move_transfers_the_connection)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  tcpstream a(pair.fds[0]);
  tcpstream b(pair.fds[1]);

  tcpstream moved(std::move(a));
  ZCHECK(moved.is_open());
  ZCHECK(!a.is_open());          // the moved-from stream must not still own it

  moved.write("moved", 5);
  moved.flush();

  char out[6];
  std::memset(out, 0, sizeof(out));
  b.read(out, 5);
  ZCHECK_STR(std::string(out, 5), "moved");

  pair.fds[0] = pair.fds[1] = Socket::INVALID_SOCKET;
}

ZTEST(SocketIO, socket_helpers)
{
  Pair pair;
  if (!pair.ok) { ZCHECK(false); return; }

  ZCHECK(Socket::is_open(pair.fds[0]));
  ZCHECK(!Socket::is_open(Socket::INVALID_SOCKET));
  ZCHECK_NOTHROW(Socket::set_blocking_mode(pair.fds[0], false));
  ZCHECK_NOTHROW(Socket::set_blocking_mode(pair.fds[0], true));
}

// End to end through the real listener, on a loopback port.
ZTEST(SocketIO, tcpserver_accepts_a_loopback_client)
{
  TCPServer server;
  const std::string service = "34821";

  std::atomic<bool> served(false);
  std::atomic<bool> listening(false);
  std::atomic<bool> bind_failed(false);

  std::thread server_thread([&] () {
      try {
	listening = true;
	server.run(TCPServer::IPV4, service, 4, 2, [&served] (Socket::handle_t handle) {
	    tcpstream client(handle, true, 5);
	    char in[5];
	    std::memset(in, 0, sizeof(in));
	    client.read(in, 4);
	    if (std::string(in, 4) == "PING") {
	      client.write("PONG", 4);
	      client.flush();
	      served = true;
	    }
	    client.close();
	  });
      } catch (...) {
	bind_failed = true;
      }
    });

  for (int i = 0; i < 100 && !listening && !bind_failed; i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  if (bind_failed) {
    // A busy port is an environment problem, not a defect under test.
    std::cout << "          (skipped: could not bind port " << service << ")" << std::endl;
    server.shutdown(true);
    server_thread.join();
    return;
  }

  bool answered = false;
  try {
    tcpstream client("127.0.0.1", service, true, 5);
    client.write("PING", 4);
    client.flush();

    char out[5];
    std::memset(out, 0, sizeof(out));
    client.read(out, 4);
    answered = (std::string(out, 4) == "PONG");
    client.close();
  } catch (const std::exception& e) {
    Zigurat::ZTest::fail(__FILE__, __LINE__, std::string("client failed: ") + e.what());
  }

  ZCHECK(answered);

  server.shutdown(true);
  server_thread.join();
  ZCHECK(served.load());
}
