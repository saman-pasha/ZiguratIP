#include "binarystream.h"
#include <limits>
#include <vector>


namespace Zigurat
{

  std::streamsize binarystream::length()
  {
    std::streampos current = this->tellg();
    this->seekg(0, std::ios_base::end);
    std::streampos length = this->tellg();
    this->seekg(current, std::ios_base::beg);
    return length;
  }
  
  binarystream::char_type binarystream::at(std::streampos pos)
  {
    std::streampos current = this->tellg();
    this->seekg(pos, std::ios_base::beg);
    char_type ch = this->get();
    this->seekg(current, std::ios_base::beg);
    return ch;
  }
  
  std::basic_istream<binarystream::char_type>& binarystream::read(std::ostream& output, std::streamsize n)
  {
    if (n <= 0) return *this;

    // Heap, not a stack array: n comes from record and certificate lengths.
    std::vector<char_type> buffer((size_t)n);
    this->read(buffer.data(), n);
    if (this->gcount() > 0) output.write(buffer.data(), this->gcount());
    return *this;
  }

  // The positional overloads name where to read from, so like at() they put the
  // get position back afterwards. Callers such as the DER encoders copy a buffer
  // out and then keep using it, and leaving it at eof breaks the next read.
  std::basic_istream<binarystream::char_type>& binarystream::read(char_type* s, std::streampos pos, std::streamsize n)
  {
    const std::streampos current = this->tellg();
    this->seekg(pos, std::ios_base::beg);
    this->read(s, n);
    this->clear();
    this->seekg(current, std::ios_base::beg);
    return *this;
  }

  std::basic_istream<binarystream::char_type>& binarystream::read(std::ostream& output, std::streampos pos, std::streamsize n)
  {
    const std::streampos current = this->tellg();
    this->seekg(pos, std::ios_base::beg);
    this->read(output, n);
    this->clear();
    this->seekg(current, std::ios_base::beg);
    return *this;
  }

  std::basic_istream<binarystream::char_type>& binarystream::getline(std::ostream& output, char_type delim)
  {
    std::string line;
    std::getline(*this, line, delim);
    output.write(line.c_str(), line.size());
    return *this;
  }

  std::basic_istream<binarystream::char_type>& binarystream::getline(std::ostream& output, int n, char_type delim)
  {
    return this->getline(output, (std::streamsize)n, delim);
  }

  std::basic_istream<binarystream::char_type>& binarystream::getline(std::ostream& output, std::streamsize n, char_type delim)
  {
    if (n <= 0) return *this;

    std::vector<char_type> buffer((size_t)n);

    // get() rather than getline(): running out of room before reaching the
    // delimiter is a normal outcome when sniffing the head of a binary blob,
    // not a failure. The length comes from gcount() so embedded NULs survive.
    this->get(buffer.data(), n, delim);
    const std::streamsize count = this->gcount();

    // Nothing extracted only means the delimiter came first, which is not an
    // error either; the caller sees an empty line.
    if (count == 0 && !this->eof()) this->clear();

    if (this->good() && this->peek() == traits_type::to_int_type(delim)) this->ignore(1);

    if (count > 0) output.write(buffer.data(), count);
    return *this;
  }

  std::streamsize binarystream::read_exact(char_type* s, std::streamsize n)
  {
    static std::streamsize prefer_length = 1024;
    std::streamsize offset = 0;
    while (!this->eof() && offset < n) {
      prefer_length = std::min(prefer_length, n - offset);
      this->read(s + offset, prefer_length);
      offset += this->gcount();
    }
    return offset;
  }
  
  std::streamsize binarystream::read_exact(std::ostream& output, std::streamsize n)
  {
    static std::streamsize prefer_length = 1024;
    std::streamsize offset = 0;
    while (!this->eof() && offset < n) {
      prefer_length = std::min(prefer_length, n - offset);
      this->read(output, prefer_length);
      offset += this->gcount();
    }
    return offset;
  }
  
  std::basic_ostream<binarystream::char_type>& binarystream::write(std::istream& input, std::streamsize n)
  {
    char_type buffer[n];
    input.read(buffer, n);
    this->write(buffer, input.gcount());
    return *this;
  }
  
  std::basic_ostream<binarystream::char_type>& binarystream::write(const char_type* s, std::streamsize n, std::streampos pos)
  {
    this->seekp(pos, std::ios_base::beg);
    this->write(s, n);
    return *this;
  }
  
  std::basic_ostream<binarystream::char_type>& binarystream::write(std::istream& input, std::streamsize n, std::streampos pos)
  {
    this->seekp(pos, std::ios_base::beg);
    char_type buffer[n];
    input.read(buffer, n);
    this->write(buffer, input.gcount());
    return *this;
  }

  std::basic_ostream<binarystream::char_type>& binarystream::fill_n(size_t n, char_type val)
  {
    for (size_t i = 0; i < n; i++) this->put(val);
    return *this;
  }

  bool binarystream::read_std_bool()
  {
    bool object;
    this->read(reinterpret_cast<char*>(&object), sizeof(bool));
    return object;
  }

  char binarystream::read_std_char()
  {
    char object;
    this->read(reinterpret_cast<char*>(&object), sizeof(char));
    return object;
  }

  int8_t binarystream::read_std_byte()
  {
    int8_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int8_t));
    return object;
  }

  uint8_t binarystream::read_std_ubyte()
  {
    uint8_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint8_t));
    return object;
  }

  std::string binarystream::read_std_string()
  {
    uint8_t length = this->read_std_ubyte();
    if (length == 0) {
      return std::string();
    } else {
      char buffer[length];
      this->read(buffer, length);
      return std::string(buffer, length);
    }
  }

  std::string binarystream::read_std_text()
  {
    uint16_t length = this->read_std_ushort();
    if (length == 0) {
      return std::string();
    } else {
      char buffer[length];
      this->read(buffer, length);
      return std::string(buffer, length);
    }
  }

  void binarystream::read_std_bool(bool& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(bool));
  }

  void binarystream::read_std_char(char& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(char));
  }

  void binarystream::read_std_byte(int8_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int8_t));
  }

  void binarystream::read_std_ubyte(uint8_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint8_t));
  }

  void binarystream::read_std_string(std::string& object)
  {
    uint8_t length = this->read_std_ubyte();
    if (length == 0) {
      object.clear();
    } else {
      char buffer[length];
      this->read(buffer, length);
      object.assign(buffer, length);
    }
  }

  void binarystream::read_std_text(std::string& object)
  {
    uint16_t length = this->read_std_ushort();
    if (length == 0) {
      object.clear();
    } else {
      char buffer[length];
      this->read(buffer, length);
      object.assign(buffer, length);
    }
  }

  void binarystream::write_std_bool(bool&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(bool));
  }

  void binarystream::write_std_char(char&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(char));
  }

  void binarystream::write_std_byte(int8_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int8_t));
  }

  void binarystream::write_std_ubyte(uint8_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint8_t));
  }

  void binarystream::write_std_string(std::string&& object)
  {
    static const std::string::size_type shift_count = (sizeof(typename std::string::size_type) * 8) - 8;
    uint8_t length = static_cast<uint8_t>(object.length() << shift_count >> shift_count);
    this->write_std_ubyte(length);
    if (length > 0) {
      this->write(object.c_str(), length);
    }
  }

  void binarystream::write_std_text(std::string&& object)
  {
    static const std::string::size_type shift_count = (sizeof(typename std::string::size_type) * 8) - 16;
    uint16_t length = static_cast<uint16_t>(object.length() << shift_count >> shift_count);
    this->write_std_ushort(length);
    if (length > 0) {
      this->write(object.c_str(), length);
    }
  }

  void binarystream::write_std_bool(const bool& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(bool));
  }

  void binarystream::write_std_char(const char& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(char));
  }

  void binarystream::write_std_byte(const int8_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int8_t));
  }

  void binarystream::write_std_ubyte(const uint8_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint8_t));
  }

  void binarystream::write_std_string(const std::string& object)
  {
    static const std::string::size_type shift_count = (sizeof(typename std::string::size_type) * 8) - 8;
    uint8_t length = static_cast<uint8_t>(object.length() << shift_count >> shift_count);
    this->write_std_ubyte(length);
    if (length > 0) {
      this->write(object.c_str(), length);
    }
  }

  void binarystream::write_std_text(const std::string& object)
  {
    static const std::string::size_type shift_count = (sizeof(typename std::string::size_type) * 8) - 16;
    uint16_t length = static_cast<uint16_t>(object.length() << shift_count >> shift_count);
    this->write_std_ushort(length);
    if (length > 0) {
      this->write(object.c_str(), length);
    }
  }

}
