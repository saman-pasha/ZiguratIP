#include "buffer.h"


namespace Zigurat
{

  size_t Buffer::_length(std::istream& file)
  {
    std::streampos current = file.tellg();
    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(current, std::ios::beg);
    return length;
  }

  Buffer::Buffer(const char* array, size_t length)
  {
    this->_stream.write(array, length);
  }

  Buffer::Buffer(const uint8_t* array, size_t length)
  {
    this->_stream.write((const char*)array, length);
  }

  Buffer::Buffer(std::initializer_list<int> list)
  {
    for (int item : list)
      this->_stream.put(item);
  }

  Buffer::Buffer(const std::string& value)
    : _stream(value)
  {

  }

  Buffer::Buffer(std::string&& value)
    : _stream(value)
  {

  }

  std::stringstream& Buffer::sstream()
  {
    return this->_stream;
  }

  size_t Buffer::length()
  {
    return this->_length(this->_stream);
  }

  uint8_t Buffer::peek()
  {
    static uint8_t byte[1];
    this->_stream.read((char*)byte, 1);
    this->_stream.seekg(-1, std::ios::cur);
    return byte[0];
  }

  uint8_t Buffer::get()
  {
    static uint8_t byte[1];
    this->_stream.read((char*)byte, 1);
    return byte[0];
  }

  uint8_t Buffer::at(std::streampos index)
  {
    std::streampos current = this->_stream.tellg();
    this->_stream.seekg(index, std::ios::beg);
    uint8_t byte = this->get();
    this->_stream.seekg(current, std::ios::beg);
    return byte;
  }

  size_t Buffer::read(char* buffer, size_t length)
  {
    this->_stream.read(buffer, length);
    return this->_stream.gcount();
  }

  size_t Buffer::read(uint8_t* buffer, size_t length)
  {
    this->_stream.read((char*)buffer, length);
    return this->_stream.gcount();
  }

  size_t Buffer::read(std::ostream& file, size_t length)
  {
    uint8_t buffer[length];
    this->_stream.read((char*)buffer, length);
    file.write((char*)buffer, length);
    return this->_stream.gcount();
  }

  size_t Buffer::read(Buffer& other, size_t length)
  {
    return this->read(other._stream, length);
  }

  size_t Buffer::read_line(Buffer& line, uint8_t byte)
  {
    uint8_t tmp;
    size_t length = 0;
    while (!this->_stream.eof()) {
      tmp = this->get();
      if (tmp == byte)
	break;
      line.put(tmp);
      length++;
    }
    return length;
  }

  size_t Buffer::dump(std::ostream& file)
  {
    size_t length = this->length();
    uint8_t buffer[length];
    std::streampos current = this->_stream.tellg();
    this->_stream.seekg(0, std::ios::beg);
    this->_stream.read((char*)buffer, length);
    file.write((char*)buffer, length);
    this->_stream.seekg(current, std::ios::beg);
    return length;
  }

  size_t Buffer::dump(Buffer& other)
  {
    return this->dump(other._stream);
  }

  std::streampos Buffer::tell_read()
  {
    return this->_stream.tellg();
  }

  void Buffer::seek_read(std::streamoff off, std::ios::seekdir way)
  {
    this->_stream.seekg(off, way);
  }

  bool Buffer::eof() const
  {
    return this->_stream.eof();
  }

  void Buffer::put(char byte)
  {
    this->_stream.put(byte);
  }

  void Buffer::put(uint8_t byte)
  {
    this->_stream.put(byte);
  }

  void Buffer::put(int byte)
  {
    this->_stream.put(byte);
  }

  void Buffer::put(unsigned int byte)
  {
    this->_stream.put(byte);
  }

  void Buffer::write(const char* buffer, size_t length)
  {
    this->_stream.write(buffer, length);
  }

  void Buffer::write(const std::string& str)
  {
    this->_stream << str;
  }

  void Buffer::write(std::string&& str)
  {
    this->_stream << str;
  }

  void Buffer::write(const uint8_t* buffer, size_t length)
  {
    this->_stream.write((char*)buffer, length);
  }

  void Buffer::write(std::istream& file, size_t length)
  {
    uint8_t buffer[length];
    file.read((char*)buffer, length);
    this->_stream.write((char*)buffer, length);
  }

  void Buffer::write(Buffer& other, size_t length)
  {
    this->write(other._stream, length);
  }

  void Buffer::load(std::istream& file)
  {
    size_t length = this->_length(file);
    uint8_t buffer[length];
    std::streampos current = file.tellg();
    file.seekg(0, std::ios::beg);
    file.read((char*)buffer, length);
    this->_stream.write((char*)buffer, length);
    file.seekg(current, std::ios::beg);
  }

  void Buffer::load(Buffer& other)
  {
    this->load(other._stream);
  }

  std::streampos Buffer::tell_write()
  {
    return this->_stream.tellp();
  }

  void Buffer::seek_write(std::streamoff off, std::ios::seekdir way)
  {
    this->_stream.seekp(off, way);
  }

  void Buffer::reset()
  {
    this->_stream.str("");
  }

  void Buffer::clear()
  {
    this->_stream.clear();
  }

  void Buffer::string(std::string data)
  {
    this->_stream.str(data);
  }

  std::string Buffer::string()
  {
    return this->_stream.str();
  }

  std::ostream& operator<<(std::ostream& out_stream, Buffer& buffer)
  {
    std::streampos current = buffer.tell_read();
    buffer.seek_read(0, std::ios::beg);
    out_stream << buffer.sstream().rdbuf();
    buffer.seek_read(current, std::ios::beg);
    return out_stream;
  }

}
