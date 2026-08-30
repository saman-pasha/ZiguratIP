#include "ztest.hpp"
#include "arraystream.hpp"
#include "arraybuf.hpp"
#include "bufferstream.hpp"
#include "filestream.hpp"
#include "mapstream.hpp"
#include <cstdio>
#include <cstring>

using namespace Zigurat;


// ---------------------------------------------------------------------------
// arraybuf: a fixed size view over a caller owned array, with the get and the
// put position moving independently over the same bytes.
// ---------------------------------------------------------------------------

ZTEST(StreamIO, arraybuf_reads_the_whole_array)
{
  char raw[] = "ZiguratIP";
  arraybuf buf;
  buf.setbuf(raw, 9);

  ZCHECK(buf.is_open());

  std::string read_back;
  for (int i = 0; i < 9; i++) {
    arraybuf::int_type ch = buf.sbumpc();
    ZCHECK(ch != arraybuf::traits_type::eof());
    read_back.push_back((char)ch);
  }

  ZCHECK_STR(read_back, "ZiguratIP");
  // Exhausted: the array cannot refill.
  ZCHECK(buf.sbumpc() == arraybuf::traits_type::eof());
}

ZTEST(StreamIO, arraybuf_write_then_rewind_and_read)
{
  char raw[16];
  std::memset(raw, 0, sizeof(raw));

  arraystream stream(raw, sizeof(raw));
  stream.write("abcdef", 6);
  ZCHECK(stream.good());
  ZCHECK_EQ((long)stream.tellp(), 6L);

  // The get position starts at zero and is independent of the put position.
  ZCHECK_EQ((long)stream.tellg(), 0L);

  char out[7];
  std::memset(out, 0, sizeof(out));
  stream.read(out, 6);
  ZCHECK_EQ((long)stream.gcount(), 6L);
  ZCHECK_STR(std::string(out, 6), "abcdef");
}

ZTEST(StreamIO, arraybuf_seek_get_and_put)
{
  char raw[11] = "0123456789";
  arraystream stream(raw, 10);

  stream.seekg(4, std::ios_base::beg);
  ZCHECK_EQ((long)stream.tellg(), 4L);
  ZCHECK_EQ(stream.get(), (int)'4');

  stream.seekg(-2, std::ios_base::end);
  ZCHECK_EQ((long)stream.tellg(), 8L);
  ZCHECK_EQ(stream.get(), (int)'8');

  stream.seekg(0, std::ios_base::beg);
  stream.seekg(3, std::ios_base::cur);
  ZCHECK_EQ((long)stream.tellg(), 3L);

  stream.seekp(2, std::ios_base::beg);
  ZCHECK_EQ((long)stream.tellp(), 2L);
  stream.put('X');
  ZCHECK_EQ(raw[2], 'X');
  ZCHECK_EQ((long)stream.tellp(), 3L);
}

ZTEST(StreamIO, arraybuf_rejects_out_of_range_seeks)
{
  char raw[8] = "1234567";
  arraybuf buf;
  buf.setbuf(raw, 8);

  ZCHECK(buf.pubseekpos(9, std::ios_base::in) == arraybuf::pos_type(arraybuf::off_type(-1)));
  ZCHECK(buf.pubseekpos(-1, std::ios_base::in) == arraybuf::pos_type(arraybuf::off_type(-1)));
  // The very end is a legal position, it just has nothing left to read.
  ZCHECK(buf.pubseekpos(8, std::ios_base::in) == arraybuf::pos_type(8));
}

ZTEST(StreamIO, arraybuf_stops_at_capacity)
{
  char raw[4];
  std::memset(raw, 0, sizeof(raw));

  arraystream stream(raw, 4);
  stream.write("abcd", 4);
  ZCHECK(stream.good());

  // One byte past the end of a fixed array must fail, not grow or corrupt.
  stream.put('e');
  stream.flush();
  ZCHECK(stream.fail());
  ZCHECK_STR(std::string(raw, 4), "abcd");
}

ZTEST(StreamIO, arraybuf_putback)
{
  char raw[6] = "hello";
  arraybuf buf;
  buf.setbuf(raw, 5);

  ZCHECK_EQ(buf.sbumpc(), (int)'h');
  ZCHECK_EQ(buf.sbumpc(), (int)'e');
  ZCHECK_EQ(buf.sputbackc('e'), (int)'e');
  ZCHECK_EQ(buf.sbumpc(), (int)'e');

  // Nothing to back up into at the very start.
  buf.pubseekpos(0, std::ios_base::in);
  ZCHECK(buf.sputbackc('z') == arraybuf::traits_type::eof());
}

ZTEST(StreamIO, arraybuf_close_releases_the_view_only)
{
  char raw[5] = "keep";
  arraybuf buf;
  buf.setbuf(raw, 4);
  ZCHECK(buf.is_open());

  buf.close();
  ZCHECK(!buf.is_open());
  ZCHECK(buf.sbumpc() == arraybuf::traits_type::eof());

  // close() must not have touched the caller's array.
  ZCHECK_STR(std::string(raw, 4), "keep");
}

ZTEST(StreamIO, arraybuf_empty_and_null_buffers_are_inert)
{
  arraybuf buf;
  ZCHECK(!buf.is_open());
  ZCHECK(buf.sbumpc() == arraybuf::traits_type::eof());
  ZCHECK(buf.sputc('x') == arraybuf::traits_type::eof());

  buf.setbuf(nullptr, 0);
  ZCHECK(!buf.is_open());
}

ZTEST(StreamIO, arraybuf_move)
{
  char raw[6] = "12345";
  arraybuf source;
  source.setbuf(raw, 5);
  ZCHECK_EQ(source.sbumpc(), (int)'1');

  arraybuf moved(std::move(source));
  ZCHECK(moved.is_open());
  ZCHECK(!source.is_open());
  // The get position travels with the buffer.
  ZCHECK_EQ(moved.sbumpc(), (int)'2');
  ZCHECK(source.sbumpc() == arraybuf::traits_type::eof());
}

// ---------------------------------------------------------------------------
// binarystream typed serialisation, exercised over the array buffer.
// ---------------------------------------------------------------------------

ZTEST(StreamIO, arraystream_std_type_roundtrip)
{
  char raw[256];
  std::memset(raw, 0, sizeof(raw));
  arraystream stream(raw, sizeof(raw));

  stream.write_std_bool(true);
  stream.write_std_char('Z');
  stream.write_std_byte((int8_t)-42);
  stream.write_std_ubyte((uint8_t)200);
  stream.write_std_short((int16_t)-1234);
  stream.write_std_ushort((uint16_t)60000);
  stream.write_std_int((int32_t)-123456789);
  stream.write_std_uint((uint32_t)4000000000u);
  stream.write_std_long((int64_t)-1234567890123LL);
  stream.write_std_ulong((uint64_t)12345678901234567890ull);
  stream.write_std_float(1.5f);
  stream.write_std_double(2.25);
  stream.write_std_string(std::string("ZiguratIP"));
  stream.write_std_text(std::string("a longer text field"));
  ZCHECK(stream.good());

  stream.seekg(0, std::ios_base::beg);

  ZCHECK_EQ(stream.read_std_bool(), true);
  ZCHECK_EQ(stream.read_std_char(), 'Z');
  ZCHECK_EQ((int)stream.read_std_byte(), -42);
  ZCHECK_EQ((int)stream.read_std_ubyte(), 200);
  ZCHECK_EQ(stream.read_std_short(), (int16_t)-1234);
  ZCHECK_EQ(stream.read_std_ushort(), (uint16_t)60000);
  ZCHECK_EQ(stream.read_std_int(), (int32_t)-123456789);
  ZCHECK_EQ(stream.read_std_uint(), (uint32_t)4000000000u);
  ZCHECK_EQ(stream.read_std_long(), (int64_t)-1234567890123LL);
  ZCHECK_EQ(stream.read_std_ulong(), (uint64_t)12345678901234567890ull);
  ZCHECK_EQ(stream.read_std_float(), 1.5f);
  ZCHECK_EQ(stream.read_std_double(), 2.25);
  ZCHECK_STR(stream.read_std_string(), "ZiguratIP");
  ZCHECK_STR(stream.read_std_text(), "a longer text field");
  ZCHECK(stream.good());
}

ZTEST(StreamIO, string_field_is_length_prefixed_by_one_octet)
{
  char raw[64];
  std::memset(raw, 0, sizeof(raw));
  arraystream stream(raw, sizeof(raw));

  stream.write_std_string(std::string("abc"));
  ZCHECK_EQ((int)(uint8_t)raw[0], 3);
  ZCHECK_STR(std::string(raw + 1, 3), "abc");

  stream.seekg(0, std::ios_base::beg);
  ZCHECK_STR(stream.read_std_string(), "abc");
}

ZTEST(StreamIO, empty_string_and_text_roundtrip)
{
  char raw[32];
  std::memset(raw, 0, sizeof(raw));
  arraystream stream(raw, sizeof(raw));

  stream.write_std_string(std::string());
  stream.write_std_text(std::string());
  stream.seekg(0, std::ios_base::beg);

  ZCHECK_STR(stream.read_std_string(), "");
  ZCHECK_STR(stream.read_std_text(), "");
}

// ---------------------------------------------------------------------------
// bufferstream and filestream wrap the standard buffers but add the same
// binarystream API, so the round trip is worth pinning down for each.
// ---------------------------------------------------------------------------

ZTEST(StreamIO, bufferstream_roundtrip)
{
  bufferstream stream;

  stream.write_std_int(987654321);
  stream.write_std_string(std::string("buffered"));
  ZCHECK(stream.good());
  ZCHECK(stream.string().size() > 0);

  stream.seekg(0, std::ios_base::beg);
  ZCHECK_EQ(stream.read_std_int(), 987654321);
  ZCHECK_STR(stream.read_std_string(), "buffered");
}

ZTEST(StreamIO, bufferstream_string_accessors)
{
  bufferstream stream;
  stream.string("preloaded");
  ZCHECK_STR(stream.string(), "preloaded");
  ZCHECK_EQ(stream.get(), (int)'p');
}

ZTEST(StreamIO, filestream_roundtrip)
{
  const std::string path = "/tmp/ziguratip-test-filestream.bin";
  std::remove(path.c_str());

  {
    filestream out(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    ZCHECK(out.is_open());
    out.write_std_ulong((uint64_t)0x0102030405060708ull);
    out.write_std_text(std::string("persisted"));
    out.flush();
    out.close();
  }

  {
    filestream in(path, std::ios_base::in | std::ios_base::binary);
    ZCHECK(in.is_open());
    ZCHECK_EQ(in.read_std_ulong(), (uint64_t)0x0102030405060708ull);
    ZCHECK_STR(in.read_std_text(), "persisted");
    in.close();
    ZCHECK(!in.is_open());
  }

  std::remove(path.c_str());
}

ZTEST(StreamIO, mapstream_roundtrip_and_grows_exactly)
{
  const std::string path = "/tmp/ziguratip-test-mapstream.bin";
  std::remove(path.c_str());

  {
    mapstream out(path, std::ios_base::out | std::ios_base::trunc);
    ZCHECK(out.is_open());
    out.write_std_ulong((uint64_t)0x0102030405060708ull);
    out.write_std_text(std::string("mapped"));
    // the file is exactly as long as what was written: 8 + 2 + 6
    ZCHECK_EQ((long)out.length(), 16L);
    // a fill is one write, and the file grows by exactly it
    out.seekp(0, std::ios_base::end);
    out.fill_n(8192, (char)0);
    ZCHECK_EQ((long)out.length(), 16L + 8192L);
    // one position, as a filebuf has: a seekp then a read reads there
    out.seekp(8, std::ios_base::beg);
    ZCHECK_STR(out.read_std_text(), "mapped");
    ZCHECK(out.sync_to_disk());
    out.close();
    ZCHECK(!out.is_open());
  }

  {
    mapstream in(path, std::ios_base::in);
    ZCHECK(in.is_open());
    ZCHECK_EQ(in.read_std_ulong(), (uint64_t)0x0102030405060708ull);
    ZCHECK_STR(in.read_std_text(), "mapped");
    ZCHECK_EQ((long)in.length(), 16L + 8192L);
    // a read past the end fails and sets eof, and clear() recovers, as
    // the engine expects of its streams
    in.seekg(16 + 8192, std::ios_base::beg);
    uint8_t byte = 7;
    in.read_std_ubyte(byte);
    ZCHECK(!in.good());
    in.clear();
    in.seekg(0, std::ios_base::beg);
    ZCHECK(in.good());
    // a reader is refused a write, loudly rather than silently
    in.write_std_ubyte((uint8_t)1);
    ZCHECK(!in.good());
    in.close();
  }

  std::remove(path.c_str());
}

ZTEST(StreamIO, mapstream_reader_sees_a_writers_growth)
{
  const std::string path = "/tmp/ziguratip-test-mapstream-grow.bin";
  std::remove(path.c_str());

  mapstream writer(path, std::ios_base::out | std::ios_base::trunc);
  writer.write_std_long((int64_t)1);
  mapstream reader(path, std::ios_base::in);
  ZCHECK_EQ(reader.read_std_long(), (int64_t)1);
  // the writer appends after the reader opened: no flush, no reopen, and
  // the reader's next read past its known end finds the new bytes
  writer.seekp(0, std::ios_base::end);
  writer.write_std_long((int64_t)2);
  ZCHECK_EQ(reader.read_std_long(), (int64_t)2);
  ZCHECK_EQ((long)reader.length(), 16L);
  reader.close();
  writer.close();
  std::remove(path.c_str());
}

ZTEST(StreamIO, mapstream_a_missing_file_is_a_failed_stream)
{
  mapstream in("/tmp/ziguratip-test-mapstream-absent.bin", std::ios_base::in);
  ZCHECK(!in.is_open());
  ZCHECK(!in.good());
}

ZTEST(StreamIO, binarystream_length_and_at)
{
  bufferstream stream;
  stream.write("0123456789", 10);
  stream.seekg(0, std::ios_base::beg);

  ZCHECK_EQ((long)stream.length(), 10L);
  ZCHECK_EQ(stream.at(0), '0');
  ZCHECK_EQ(stream.at(9), '9');
  // at() must leave the get position where it found it.
  ZCHECK_EQ((long)stream.tellg(), 0L);
}

ZTEST(StreamIO, binarystream_fill_n)
{
  bufferstream stream;
  stream.fill_n(5, 'x');
  ZCHECK_STR(stream.string(), "xxxxx");
}
