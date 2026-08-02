#include "zlibhelper.hpp"
#include "zlib.h"
#include "compressionexception.hpp"
#include <cassert>


namespace Zigurat
{

  const int ZLib::CHUNK = 262144;

  int ZLib::def(std::istream& source, std::streamsize length, std::ostream& dest, int level)
  {
    std::streamsize count = 0;
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    do {
      source.read((char*)in, std::min<std::streamsize>(length - count, CHUNK));
      strm.avail_in = source.gcount();
      count += strm.avail_in;
      if (source.fail()) {
	(void)deflateEnd(&strm);
	return Z_ERRNO;
      }
      flush = (strm.avail_in == 0 || source.eof()) ? Z_FINISH : Z_NO_FLUSH;
      strm.next_in = in;

      do {
	strm.avail_out = CHUNK;
	strm.next_out = out;

	ret = deflate(&strm, flush);
	assert(ret != Z_STREAM_ERROR);

	have = CHUNK - strm.avail_out;
	dest.write((const char*)out, have);
	if (dest.fail()) {
	  (void)deflateEnd(&strm);
	  return Z_ERRNO;
	}

      } while (strm.avail_out == 0);
      assert(strm.avail_in == 0);

    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);

    (void)deflateEnd(&strm);
    return Z_OK;
  }

  int ZLib::inf(std::istream& source, std::streamsize length, std::ostream& dest)
  {
    std::streamsize count = 0;
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
      return ret;

    do {
      source.read((char*)in, std::min<std::streamsize>(length - count, CHUNK));
      strm.avail_in = source.gcount();
      count += strm.avail_in;
      if (source.fail()) {
	(void)inflateEnd(&strm);
	return Z_ERRNO;
      }
      if (strm.avail_in == 0)
	break;
      strm.next_in = in;

      do {
	strm.avail_out = CHUNK;
	strm.next_out = out;

	ret = inflate(&strm, Z_NO_FLUSH);
	assert(ret != Z_STREAM_ERROR);
	switch (ret) {
	case Z_NEED_DICT:
	  ret = Z_DATA_ERROR;
	case Z_DATA_ERROR:
	case Z_MEM_ERROR:
	  (void)inflateEnd(&strm);
	  return ret;
	}

	have = CHUNK - strm.avail_out;
	dest.write((const char*)out, have);
	if (dest.fail()) {
	  (void)inflateEnd(&strm);
	  return Z_ERRNO;
	}

      } while (strm.avail_out == 0);

    } while (ret != Z_STREAM_END);

    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
  }

  void ZLib::zerr(int ret)
  {
    switch (ret) {
    case Z_ERRNO:
      throw CompressionException("zlib I/O error");
    case Z_STREAM_ERROR:
      throw CompressionException("zlib invalid compression level");
    case Z_DATA_ERROR:
      throw CompressionException("zlib invalid or incomplete deflate data");
    case Z_MEM_ERROR:
      throw CompressionException("zlib out of memory");
    case Z_VERSION_ERROR:
      throw CompressionException("zlib version mismatch");
    default:
      throw CompressionException("zlib unknown error");
    }
  }

  void ZLib::compress(Algorithm algorithm, std::istream& input, std::streamsize length, std::ostream& output, int level)
  {
    int ret = 0;
    switch (algorithm) {
    case ZLib::ZLIB:
      throw CompressionException("unhandled compression algorithm");
    case ZLib::DEFLATE:
      ret = ZLib::def(input, length, output, level);
      break;
    case ZLib::GZIP:
      throw CompressionException("unhandled compression algorithm");
    default:
      throw CompressionException("unknown compression algorithm");
    }

    if (ret != Z_OK) ZLib::zerr(ret);
  }

  void ZLib::decompress(Algorithm algorithm, std::istream& input, std::streamsize length, std::ostream& output)
  {
    int ret = 0;
    switch (algorithm) {
    case ZLib::ZLIB:
      throw CompressionException("unhandled compression algorithm");
    case ZLib::DEFLATE:
      ret = ZLib::inf(input, length, output);
      break;
    case ZLib::GZIP:
      throw CompressionException("unhandled compression algorithm");
    default:
      throw CompressionException("unknown compression algorithm");
    }

    if (ret != Z_OK) ZLib::zerr(ret);
  }

}
