#include "arraystream.hpp"


namespace Zigurat
{

  arraystream::arraystream(char *s, size_t n)
    : hbostream(new arraybuf())
  {
    dynamic_cast<arraybuf*>(this->rdbuf())->setbuf(s, n);
  }
  
  arraystream::arraystream(uint8_t *s, size_t n)
    : hbostream(new arraybuf())
  {
    dynamic_cast<arraybuf*>(this->rdbuf())->setbuf((char*)s, n);
  }
  
  arraystream& arraystream::operator=(arraystream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  void arraystream::swap(arraystream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  arraystream::~arraystream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
