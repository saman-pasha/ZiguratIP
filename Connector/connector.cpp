#include "connector.hpp"
#include "connectorexception.hpp"
#include "types.hpp"
#include "utility.hpp"
#include "configuration.hpp"
#include "tcpstream.hpp"
#include "tlsstream.hpp"


namespace Zigurat
{

  Connector::Connector()
  {

  }

  // A file name from the configuration: absolute is taken as given, anything
  // else is looked for beside the configuration itself.
  static std::string connector_file(const std::string& name)
  {
    if (name.empty()) return "";
    if (name[0] == '/') return name;

    std::string beside = Utility::config_path("cert/" + name);
    if (!beside.empty()) return beside;

    return name;
  }

  void Connector::open()
  {
    std::string conf_path = Utility::config_path("connector.conf");
    if (conf_path.empty())
      throw ConnectorException("connector configuration not found");

    Configuration conf(conf_path);

    std::string host, service, tmp;
    if (!conf.get("/HOST/", host))
      throw ConnectorException("connector configuration TCP host not found");

    // PORT is what ziguratip.conf calls it, SERVICE is what this file used to;
    // both are accepted so an existing connector.conf keeps working.
    if (!conf.get("/PORT/", service) && !conf.get("/SERVICE/", service))
      throw ConnectorException("connector configuration TCP service not found");

    host    = Utility::trim(host);
    service = Utility::trim(service);

    bool blocking_mode = true;
    int  timeout = 0;

    if (conf.get("/BLOCKING_MODE/", tmp)) {
      tmp = Utility::to_upper(Utility::trim(tmp));
      blocking_mode = (tmp != "FALSE" && tmp != "0");
    }

    if (conf.get("/TIMEOUT/", tmp)) {
      std::stringstream ss(tmp);
      ss >> timeout;
    }

    bool tls_mode = false;
    if (conf.get("/TLS_MODE/", tmp)) {
      tmp = Utility::to_upper(Utility::trim(tmp));
      if (tmp == "TRUE")
	tls_mode = true;
      else if (tmp == "FALSE")
	tls_mode = false;
      else
	throw ConnectorException("invalid value for 'TLS_MODE' in connector configuration");
    }

    // This used to read the configuration and then return without connecting at
    // all, leaving the caller with a Connector that had no stream.
    if (!tls_mode) {
      this->open(host, service, blocking_mode, timeout);
      return;
    }

    TLS::HandshakeParameters params;
    params.protocol_version = TLS::VERSION_1_2;
    params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_256_CBC_SHA256);
    params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_128_CBC_SHA256);
    params.compression_methods.push_back(TLS::CompressionMethod::NONE);

    if (conf.get("/CERTIFICATE/", tmp))
      params.credentials.certificate = connector_file(Utility::trim(tmp));
    if (conf.get("/PRIVATE_KEY/", tmp))
      params.credentials.private_key = connector_file(Utility::trim(tmp));
    if (conf.get("/PRIVATE_KEY_CIPHER/", tmp))
      params.credentials.private_key_cipher = tmp;
    if (conf.get("/AUTHORITY/", tmp))
      params.credentials.authority = connector_file(Utility::trim(tmp));

    if (params.credentials.certificate.empty()
	|| params.credentials.private_key.empty()
	|| params.credentials.authority.empty())
      throw ConnectorException("connector TLS_MODE is on, but CERTIFICATE,"
			       " PRIVATE_KEY and AUTHORITY are not all set");

    this->open(params, host, service, blocking_mode, timeout);
  }

  void Connector::open(std::string host, std::string service, bool blocking_mode, int timeout)
  {
    if (!this->is_open()) {
      if (this->_stream != nullptr) {
	delete this->_stream;
	this->_stream = nullptr;
      }
      this->_stream = new tcpstream(host, service, blocking_mode, timeout);      
      this->_transaction_id = this->_stream->read_std_size();
    }
  }

  void Connector::open(const TLS::HandshakeParameters& params, std::string host, std::string service,
		       bool blocking_mode, int timeout)
  {
    if (!this->is_open()) {
      if (this->_stream != nullptr) {
	delete this->_stream;
	this->_stream = nullptr;
      }

      tlsstream* secure = new tlsstream();
      this->_stream = secure;

      // The handshake happens here: if the server will not have this client, or
      // this client will not have that server, open throws and there is no
      // connection to speak over.
      secure->open(params, host, service, blocking_mode, timeout);

      this->_transaction_id = this->_stream->read_std_size();
    }
  }

  bool Connector::is_open()
  {
    // open() asks this before there is a stream at all, so the null case has to
    // answer rather than dereference.
    tcpstream* stream = dynamic_cast<tcpstream*>(this->_stream);
    return (stream != nullptr) && stream->is_open();
  }

  void Connector::close()
  {
    if (this->_stream != nullptr) {
      dynamic_cast<tcpstream*>(this->_stream)->close();
      delete this->_stream;
      this->_stream = nullptr;
    }
    this->_transaction_id = 0;
  }

  size_t Connector::transaction_id()
  {
    return this->_transaction_id;
  }

  ULong Connector::TRANSACTION_ID()
  {
    return (uint64_t)this->_transaction_id;
  }

  binarystream& Connector::stream()
  {
    if (this->_stream == nullptr)
      throw ConnectorException("Connector is not connected");
    return *this->_stream;
  }

  ResultType Connector::result()
  {
    ResultType type = (ResultType)this->_stream->read_std_ubyte();
    switch (type) {
    case ResultType::EXCEPTION_THROWN:
      throw ConnectorException(this->_stream->read_std_string());
    default:
      return type;
    }
  }

  void Connector::function(std::string func)
  {
    this->_stream->write_std_string(func);
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  std::string Connector::echo(std::string text)
  {
    this->function("echo");
    this->_stream->write_std_string(text);
    return this->_stream->read_std_string();
  }
  
  void Connector::compile(std::string code)
  {
    this->function("compile");
    this->_stream->write_std_text(code);
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  void Connector::call(std::string procedure)
  {
    this->function("call");
    this->_stream->write_std_string(procedure);
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  void Connector::auto_commit(bool auto_commit)
  {
    this->function("auto_commit");
    this->_stream->write_std_bool(auto_commit);
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  void Connector::isolate(IsolationLevel isolation_level)
  {
    this->function("isolate");
    this->_stream->write_std_ubyte((uint8_t)isolation_level);
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  std::vector<std::string> Connector::columns()
  {
    return Utility::split(this->_stream->read_std_string(), ',');
  }
  
  void Connector::commit()
  {
    this->function("commit");
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  void Connector::rollback()
  {
    this->function("rollback");
    if (this->result() != ResultType::SUCCESSFUL_DONE) {
      throw ConnectorException(this->_stream->read_std_string());
    }
  }

  void Connector::fetch()
  {

  }
    
  Bool Connector::read_bool()
  {
    return this->read<Bool>();
  }

  Char Connector::read_char()
  {
    return this->read<Char>();
  }

  Byte Connector::read_byte()
  {
    return this->read<Byte>();
  }

  UByte Connector::read_ubyte()
  {
    return this->read<UByte>();
  }

  Short Connector::read_short()
  {
    return this->read<Short>();
  }

  UShort Connector::read_ushort()
  {
    return this->read<UShort>();
  }

  Int Connector::read_int()
  {
    return this->read<Int>();
  }

  UInt Connector::read_uint()
  {
    return this->read<UInt>();
  }

  Long Connector::read_long()
  {
    return this->read<Long>();
  }

  ULong Connector::read_ulong()
  {
    return this->read<ULong>();
  }

  Float Connector::read_float()
  {
    return this->read<Float>();
  }

  Double Connector::read_double()
  {
    return this->read<Double>();
  }

  Real Connector::read_real()
  {
    return this->read<Real>();
  }

  Timestamp Connector::read_timestamp()
  {
    return this->read<Timestamp>();
  }

  String Connector::read_string()
  {
    return this->read<String>();
  }

  Text Connector::read_text()
  {
    return this->read<Text>();
  }

  void Connector::read_bool(Bool& aBool)
  {
    this->read(aBool);
  }

  void Connector::read_char(Char& aChar)
  {
    this->read(aChar);
  }

  void Connector::read_byte(Byte& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_ubyte(UByte& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_short(Short& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_ushort(UShort& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_int(Int& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_uint(UInt& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_long(Long& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_ulong(ULong& aInt)
  {
    this->read(aInt);
  }

  void Connector::read_float(Float& aFloat)
  {
    this->read(aFloat);
  }

  void Connector::read_double(Double& aDouble)
  {
    this->read(aDouble);
  }

  void Connector::read_real(Real& aReal)
  {
    this->read(aReal);
  }

  void Connector::read_timestamp(Timestamp& aTime)
  {
    this->read(aTime);
  }

  void Connector::read_string(String& aString)
  {
    this->read(aString);
  }

  void Connector::read_text(Text& aString)
  {
    this->read(aString);
  }

  void Connector::write_bool(const Bool& aBool)
  {
    return this->write<Bool>(aBool);
  }

  void Connector::write_char(const Char& aChar)
  {
    return this->write<Char>(aChar);
  }

  void Connector::write_byte(const Byte& aInt)
  {
    return this->write<Byte>(aInt);
  }

  void Connector::write_ubyte(const UByte& aInt)
  {
    return this->write<UByte>(aInt);
  }

  void Connector::write_short(const Short& aInt)
  {
    return this->write<Short>(aInt);
  }

  void Connector::write_ushort(const UShort& aInt)
  {
    return this->write<UShort>(aInt);
  }

  void Connector::write_int(const Int& aInt)
  {
    return this->write<Int>(aInt);
  }

  void Connector::write_uint(const UInt& aInt)
  {
    return this->write<UInt>(aInt);
  }

  void Connector::write_long(const Long& aInt)
  {
    return this->write<Long>(aInt);
  }

  void Connector::write_ulong(const ULong& aInt)
  {
    return this->write<ULong>(aInt);
  }

  void Connector::write_float(const Float& aFloat)
  {
    return this->write<Float>(aFloat);
  }

  void Connector::write_double(const Double& aDouble)
  {
    return this->write<Double>(aDouble);
  }

  void Connector::write_real(const Real& aReal)
  {
    return this->write<Real>(aReal);
  }

  void Connector::write_timestamp(const Timestamp& aTime)
  {
    return this->write<Timestamp>(aTime);
  }

  void Connector::write_string(const String& aString)
  {
    return this->write<String>(aString);
  }

  void Connector::write_text(const Text& aString)
  {
    return this->write<Text>(aString);
  }

  void Connector::write_bool(Bool&& aBool)
  {
    this->write(std::forward<Bool&&>(aBool));
  }

  void Connector::write_char(Char&& aChar)
  {
    this->write(std::forward<Char&&>(aChar));
  }

  void Connector::write_byte(Byte&& aInt)
  {
    this->write(std::forward<Byte&&>(aInt));
  }

  void Connector::write_ubyte(UByte&& aInt)
  {
    this->write(std::forward<UByte&&>(aInt));
  }

  void Connector::write_short(Short&& aInt)
  {
    this->write(std::forward<Short&&>(aInt));
  }

  void Connector::write_ushort(UShort&& aInt)
  {
    this->write(std::forward<UShort&&>(aInt));
  }

  void Connector::write_int(Int&& aInt)
  {
    this->write(std::forward<Int&&>(aInt));
  }

  void Connector::write_uint(UInt&& aInt)
  {
    this->write(std::forward<UInt&&>(aInt));
  }

  void Connector::write_long(Long&& aInt)
  {
    this->write(std::forward<Long&&>(aInt));
  }

  void Connector::write_ulong(ULong&& aInt)
  {
    this->write(std::forward<ULong&&>(aInt));
  }

  void Connector::write_float(Float&& aFloat)
  {
    this->write(std::forward<Float&&>(aFloat));
  }

  void Connector::write_double(Double&& aDouble)
  {
    this->write(std::forward<Double&&>(aDouble));
  }

  void Connector::write_real(Real&& aReal)
  {
    this->write(std::forward<Real&&>(aReal));
  }

  void Connector::write_timestamp(Timestamp&& aTime)
  {
    this->write(std::forward<Timestamp&&>(aTime));
  }

  void Connector::write_string(String&& aString)
  {
    this->write(std::forward<String&&>(aString));
  }

  void Connector::write_text(Text&& aString)
  {
    this->write(std::forward<Text&&>(aString));
  }

  Bool Connector::READ_BOOL()
  {
    return this->READ<Bool>();
  }

  Char Connector::READ_CHAR()
  {
    return this->READ<Char>();
  }

  Byte Connector::READ_BYTE()
  {
    return this->READ<Byte>();
  }

  UByte Connector::READ_UBYTE()
  {
    return this->READ<UByte>();
  }

  Short Connector::READ_SHORT()
  {
    return this->READ<Short>();
  }

  UShort Connector::READ_USHORT()
  {
    return this->READ<UShort>();
  }

  Int Connector::READ_INT()
  {
    return this->READ<Int>();
  }

  UInt Connector::READ_UINT()
  {
    return this->READ<UInt>();
  }

  Long Connector::READ_LONG()
  {
    return this->READ<Long>();
  }

  ULong Connector::READ_ULONG()
  {
    return this->READ<ULong>();
  }

  Float Connector::READ_FLOAT()
  {
    return this->READ<Float>();
  }

  Double Connector::READ_DOUBLE()
  {
    return this->READ<Double>();
  }

  Real Connector::READ_REAL()
  {
    return this->READ<Real>();
  }

  Timestamp Connector::READ_TIMESTAMP()
  {
    return this->READ<Timestamp>();
  }

  String Connector::READ_STRING()
  {
    return this->READ<String>();
  }

  Text Connector::READ_TEXT()
  {
    return this->READ<Text>();
  }

  void Connector::READ_BOOL(Bool& aBool)
  {
    this->READ(aBool);
  }

  void Connector::READ_CHAR(Char& aChar)
  {
    this->READ(aChar);
  }

  void Connector::READ_BYTE(Byte& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_UBYTE(UByte& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_SHORT(Short& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_USHORT(UShort& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_INT(Int& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_UINT(UInt& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_LONG(Long& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_ULONG(ULong& aInt)
  {
    this->READ(aInt);
  }

  void Connector::READ_FLOAT(Float& aFloat)
  {
    this->READ(aFloat);
  }

  void Connector::READ_DOUBLE(Double& aDouble)
  {
    this->READ(aDouble);
  }

  void Connector::READ_REAL(Real& aReal)
  {
    this->READ(aReal);
  }

  void Connector::READ_TIMESTAMP(Timestamp& aTime)
  {
    this->READ(aTime);
  }

  void Connector::READ_STRING(String& aString)
  {
    this->READ(aString);
  }

  void Connector::READ_TEXT(Text& aString)
  {
    this->READ(aString);
  }

  void Connector::WRITE_BOOL(const Bool& aBool)
  {
    return this->WRITE<Bool>(aBool);
  }

  void Connector::WRITE_CHAR(const Char& aChar)
  {
    return this->WRITE<Char>(aChar);
  }

  void Connector::WRITE_BYTE(const Byte& aInt)
  {
    return this->WRITE<Byte>(aInt);
  }

  void Connector::WRITE_UBYTE(const UByte& aInt)
  {
    return this->WRITE<UByte>(aInt);
  }

  void Connector::WRITE_SHORT(const Short& aInt)
  {
    return this->WRITE<Short>(aInt);
  }

  void Connector::WRITE_USHORT(const UShort& aInt)
  {
    return this->WRITE<UShort>(aInt);
  }

  void Connector::WRITE_INT(const Int& aInt)
  {
    return this->WRITE<Int>(aInt);
  }

  void Connector::WRITE_UINT(const UInt& aInt)
  {
    return this->WRITE<UInt>(aInt);
  }

  void Connector::WRITE_LONG(const Long& aInt)
  {
    return this->WRITE<Long>(aInt);
  }

  void Connector::WRITE_ULONG(const ULong& aInt)
  {
    return this->WRITE<ULong>(aInt);
  }

  void Connector::WRITE_FLOAT(const Float& aFloat)
  {
    return this->WRITE<Float>(aFloat);
  }

  void Connector::WRITE_DOUBLE(const Double& aDouble)
  {
    return this->WRITE<Double>(aDouble);
  }

  void Connector::WRITE_REAL(const Real& aReal)
  {
    return this->WRITE<Real>(aReal);
  }

  void Connector::WRITE_TIMESTAMP(const Timestamp& aTime)
  {
    return this->WRITE<Timestamp>(aTime);
  }

  void Connector::WRITE_STRING(const String& aString)
  {
    return this->WRITE<String>(aString);
  }

  void Connector::WRITE_TEXT(const Text& aString)
  {
    return this->WRITE<Text>(aString);
  }

  void Connector::WRITE_BOOL(Bool&& aBool)
  {
    this->WRITE(std::forward<Bool&&>(aBool));
  }

  void Connector::WRITE_CHAR(Char&& aChar)
  {
    this->WRITE(std::forward<Char&&>(aChar));
  }

  void Connector::WRITE_BYTE(Byte&& aInt)
  {
    this->WRITE(std::forward<Byte&&>(aInt));
  }

  void Connector::WRITE_UBYTE(UByte&& aInt)
  {
    this->WRITE(std::forward<UByte&&>(aInt));
  }

  void Connector::WRITE_SHORT(Short&& aInt)
  {
    this->WRITE(std::forward<Short&&>(aInt));
  }

  void Connector::WRITE_USHORT(UShort&& aInt)
  {
    this->WRITE(std::forward<UShort&&>(aInt));
  }

  void Connector::WRITE_INT(Int&& aInt)
  {
    this->WRITE(std::forward<Int&&>(aInt));
  }

  void Connector::WRITE_UINT(UInt&& aInt)
  {
    this->WRITE(std::forward<UInt&&>(aInt));
  }

  void Connector::WRITE_LONG(Long&& aInt)
  {
    this->WRITE(std::forward<Long&&>(aInt));
  }

  void Connector::WRITE_ULONG(ULong&& aInt)
  {
    this->WRITE(std::forward<ULong&&>(aInt));
  }

  void Connector::WRITE_FLOAT(Float&& aFloat)
  {
    this->WRITE(std::forward<Float&&>(aFloat));
  }

  void Connector::WRITE_DOUBLE(Double&& aDouble)
  {
    this->WRITE(std::forward<Double&&>(aDouble));
  }

  void Connector::WRITE_REAL(Real&& aReal)
  {
    this->WRITE(std::forward<Real&&>(aReal));
  }

  void Connector::WRITE_TIMESTAMP(Timestamp&& aTime)
  {
    this->WRITE(std::forward<Timestamp&&>(aTime));
  }

  void Connector::WRITE_STRING(String&& aString)
  {
    this->WRITE(std::forward<String&&>(aString));
  }

  void Connector::WRITE_TEXT(Text&& aString)
  {
    this->WRITE(std::forward<Text&&>(aString));
  }

  Connector::~Connector()
  {
    this->close();
  }
	
}
