
#ifndef __ARRAYBUF_HPP__
#define __ARRAYBUF_HPP__

#include <streambuf>

namespace Zigurat
{

  class arraybuf : public std::basic_streambuf<char>
  {
  protected:
    char_type      *_buffer;   // not owned: the caller keeps the array alive
    std::streamsize _length;

    void pbump_off(off_type);

  public:
    arraybuf();
    arraybuf(const arraybuf&) = delete;
    arraybuf(arraybuf&&);

    arraybuf& operator=(const arraybuf&) = delete;
    arraybuf& operator=(arraybuf&&);

    virtual arraybuf* setbuf(char_type*, std::streamsize) override;
    virtual pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    virtual pos_type seekpos(pos_type, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    virtual int sync() override;
    virtual std::streamsize showmanyc() override;
    virtual int_type underflow() override;
    virtual int_type overflow(int_type = traits_type::eof()) override;
    virtual int_type pbackfail(int_type = traits_type::eof()) override;

    virtual bool is_open() const;
    virtual arraybuf* close();
    virtual ~arraybuf();
  };

}

#endif // __ARRAYBUF_HPP__
