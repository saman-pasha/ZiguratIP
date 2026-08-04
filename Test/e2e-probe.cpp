// A client for the end-to-end scripts.
//
// Opens a connection -- secure with the certificate it is given, or plain when
// given "-" instead -- tries one thing, and says in one line whether the server
// allowed it. What these scripts are after happens between two processes: a
// handshake refuses before a request can be sent, a refusal later arrives as a
// protocol error or an HTTP status, and a server that dies answers nothing at
// all. None of it can be seen from inside the server's own test binary.
//
// Always exits 0: the script compares the line, and a non-zero exit under
// "set -e" would end the run before it could.

#include "connector.hpp"
#include "connectorexception.hpp"
#include "zexception.hpp"
#include "tlsstream.hpp"
#include "types.hpp"
#include "tls.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

using namespace Zigurat;


namespace
{
  // The binary protocol, through the Connector, which is what a real client
  // uses.
  void speak_zigurat(bool secure, const TLS::HandshakeParameters& params, const std::string& host,
		     const std::string& port, const std::string& verb, const std::string& argument)
  {
    Connector connector;

    // With a timeout, so a probe that gets no answer says so instead of
    // wedging the script that ran it.
    if (secure) connector.open(params, host, port, true, 20);
    else        connector.open(host, port, true, 20);

    if (verb == "connect") {
      // Getting here at all is the answer: the handshake is where an
      // unregistered subject is turned away.

    } else if (verb == "call") {
      connector.call(argument);

      // A call is a conversation: the server answers with a cursor, whatever
      // rows or return value the object produced, and then it is done. Leaving
      // any of it unread desynchronises the protocol, and the next thing sent
      // waits for a byte that already went past.
      //
      // A return value is read as a Long, which is what the procedures this is
      // pointed at return. It is a probe for one script, not a general client.
      Long returned(0);
      for (ResultType r = connector.result(); r != ResultType::SUCCESSFUL_DONE; r = connector.result()) {
	if (r == ResultType::CURSOR_OPEN) connector.columns();
	else if (r == ResultType::RETURN_VALUE) connector.fetch(returned);
      }

      connector.commit();

    } else if (verb == "compile") {
      std::ifstream source(argument);
      if (!source.good()) throw ConnectorException("cannot read " + argument);

      std::ostringstream all;
      all << source.rdbuf();
      connector.compile(all.str());

    } else {
      throw ConnectorException("unknown verb " + verb);
    }

    connector.close();
    std::cout << "OK" << std::endl;
  }

  // Zeytun over the same secure transport. A tlsstream is a stream, so the
  // request goes down it as text; nothing here needs an HTTP client.
  void speak_http(const TLS::HandshakeParameters& params, const std::string& host,
		  const std::string& port, const std::string& path)
  {
    tlsstream secure;
    secure.open(params, host, port, true, 20);

    secure << "GET " << path << " HTTP/1.1\r\n"
	   << "Host: " << host << ":" << port << "\r\n"
	   << "Connection: close\r\n\r\n";
    secure.flush();

    std::string status;
    std::getline(secure, status);
    while (!status.empty() && (status[status.size() - 1] == '\r' || status[status.size() - 1] == '\n'))
      status.erase(status.size() - 1);

    if (status.empty()) throw ConnectorException("no answer");

    // "HTTP/1.1 200 OK" -> "200". The script compares the code, so the reason
    // phrase is dropped rather than matched on.
    std::istringstream parts(status);
    std::string version, code;
    parts >> version >> code;

    secure.close();
    std::cout << "HTTP " << code << std::endl;
  }
}


int main(int argc, char* argv[])
{
  if (argc < 7) {
    std::cout << "USAGE" << std::endl;
    std::cout << "  e2e-probe <cert> <key> <authority> <host> <port> "
	      << "connect|call|compile|page [argument]" << std::endl;
    std::cout << "  e2e-probe - - - <host> <port> "
	      << "connect|call|compile [argument]      (in the clear)" << std::endl;
    return 0;
  }

  // Three dashes where the material would go: no certificate, so no handshake.
  const bool secure = (std::string(argv[1]) != "-");

  TLS::HandshakeParameters params;
  params.protocol_version = TLS::VERSION_1_2;
  params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_256_CBC_SHA256);
  params.compression_methods.push_back(TLS::CompressionMethod::NONE);
  params.credentials.certificate = argv[1];
  params.credentials.private_key = argv[2];
  params.credentials.authority   = argv[3];

  const std::string host(argv[4]), port(argv[5]), verb(argv[6]);
  const std::string argument = (argc > 7) ? argv[7] : "";

  try {
    if (verb == "page")
      speak_http(params, host, port, argument);
    else
      speak_zigurat(secure, params, host, port, verb, argument);

  } catch (const std::exception& error) {
    // One line, so the script can grep it. Newlines in a server message would
    // otherwise split one answer into several.
    std::string message(error.what());
    for (size_t i = 0; i < message.size(); i++)
      if (message[i] == '\n' || message[i] == '\r') message[i] = ' ';

    std::cout << "REFUSED " << message << std::endl;

  } catch (...) {
    std::cout << "REFUSED unknown" << std::endl;
  }

  return 0;
}
