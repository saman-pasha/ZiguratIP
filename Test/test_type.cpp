#include "ztest.hpp"
#include "types.hpp"
#include "typevector.hpp"
#include "arraystream.hpp"
#include <cstring>

using namespace Zigurat;


ZTEST(Type, integers_carry_their_value)
{
  Int i(42);
  ZCHECK_EQ(i.value(), 42);
  ZCHECK(!i.is_null().value());
  ZCHECK(i == 42);
  ZCHECK(i != 43);

  Long l((int64_t)-9000000000LL);
  ZCHECK_EQ(l.value(), (int64_t)-9000000000LL);

  UInt u((uint32_t)4000000000u);
  ZCHECK_EQ(u.value(), (uint32_t)4000000000u);

  Short s((int16_t)-300);
  ZCHECK_EQ(s.value(), (int16_t)-300);
}

ZTEST(Type, null_is_distinct_from_zero)
{
  Int nothing(nullptr);
  ZCHECK(nothing.is_null().value());

  Int zero(0);
  ZCHECK(!zero.is_null().value());

  // Reading through a null must not hand back a stale value.
  ZCHECK_THROWS(nothing.value());
}

ZTEST(Type, set_null_clears_a_value)
{
  Int i(7);
  ZCHECK(!i.is_null().value());
  i.set_null();
  ZCHECK(i.is_null().value());
}

ZTEST(Type, assignment_and_copy)
{
  Int a(10);
  Int b(a);
  ZCHECK_EQ(b.value(), 10);

  Int c(0);
  c = a;
  ZCHECK_EQ(c.value(), 10);

  c = 99;
  ZCHECK_EQ(c.value(), 99);
  ZCHECK_EQ(a.value(), 10);   // the copy is independent
}

ZTEST(Type, comparison_operators)
{
  Int small(1);
  Int large(2);

  ZCHECK(small < large);
  ZCHECK(small <= large);
  ZCHECK(large > small);
  ZCHECK(large >= small);
  ZCHECK(!(small > large));
}

ZTEST(Type, arithmetic_operators)
{
  Int a(10);
  Int b(3);

  ZCHECK_EQ((a + b).value(), 13);
  ZCHECK_EQ((a - b).value(), 7);
  ZCHECK_EQ((a * b).value(), 30);
  ZCHECK_EQ((a / b).value(), 3);
  ZCHECK_EQ((a % b).value(), 1);
}

ZTEST(Type, strings_and_text)
{
  String s(std::string("ZiguratIP"));
  ZCHECK_STR(s.to_std_string(), "ZiguratIP");
  ZCHECK_EQ((uint64_t)s.size().value(), (uint64_t)9);
  ZCHECK_EQ((uint64_t)s.length().value(), (uint64_t)9);

  Text t(std::string("a longer body of text"));
  ZCHECK_STR(t.to_std_string(), "a longer body of text");
  ZCHECK_EQ((uint64_t)t.length().value(), (uint64_t)21);
}

ZTEST(Type, booleans)
{
  Bool yes(true);
  Bool no(false);

  ZCHECK(yes.value());
  ZCHECK(!no.value());
  ZCHECK(!yes.is_null().value());

  Bool unknown(nullptr);
  ZCHECK(unknown.is_null().value());
}

ZTEST(Type, floating_point)
{
  Float f(1.5f);
  ZCHECK_EQ(f.value(), 1.5f);

  Double d(2.25);
  ZCHECK_EQ(d.value(), 2.25);
}

ZTEST(Type, type_discriminator_bytes_are_distinct)
{
  Int i(1);
  Long l((int64_t)1);
  String s(std::string("x"));
  Bool b(true);

  ZCHECK(i.tdb() != l.tdb());
  ZCHECK(i.tdb() != s.tdb());
  ZCHECK(i.tdb() != b.tdb());
  ZCHECK(l.tdb() != s.tdb());
}

ZTEST(Type, to_string_renders_the_value)
{
  Int i(-42);
  ZCHECK_STR(i.to_std_string(), "-42");

  Bool b(true);
  ZCHECK(b.to_std_string().size() > 0);

  Int nothing(nullptr);
  ZCHECK_STR(nothing.to_std_string(), Object::NULL_STRING);
}

ZTEST(Type, pack_size_is_positive)
{
  Int i(1);
  ZCHECK(i.pack_size() > 0);

  String s(std::string("ZiguratIP"));
  ZCHECK(s.pack_size() > 0);
}

// Every type serialises itself through binarystream, which is how rows reach
// both the storage engine and the wire.
ZTEST(Type, binary_roundtrip_through_a_stream)
{
  char raw[512];
  std::memset(raw, 0, sizeof(raw));
  arraystream stream(raw, sizeof(raw));

  Int    in_int(-123456);
  Long   in_long((int64_t)987654321012LL);
  String in_string(std::string("row value"));
  Bool   in_bool(true);

  stream << in_int << in_long << in_string << in_bool;
  ZCHECK(stream.good());

  stream.seekg(0, std::ios_base::beg);

  Int    out_int(0);
  Long   out_long((int64_t)0);
  String out_string(std::string(""));
  Bool   out_bool(false);

  stream >> out_int >> out_long >> out_string >> out_bool;

  ZCHECK_EQ(out_int.value(), -123456);
  ZCHECK_EQ(out_long.value(), (int64_t)987654321012LL);
  ZCHECK_STR(out_string.to_std_string(), "row value");
  ZCHECK(out_bool.value());
}

ZTEST(Type, null_survives_a_binary_roundtrip)
{
  char raw[128];
  std::memset(raw, 0, sizeof(raw));
  arraystream stream(raw, sizeof(raw));

  Int in_null(nullptr);
  stream << in_null;

  stream.seekg(0, std::ios_base::beg);

  Int out(5);
  stream >> out;
  ZCHECK(out.is_null().value());
}

ZTEST(Type, vector_of_values)
{
  Vector<Int> values;
  values.resize(ULong((uint64_t)3));
  values.SET(ULong((uint64_t)0), Int(1));
  values.SET(ULong((uint64_t)1), Int(2));
  values.SET(ULong((uint64_t)2), Int(3));

  ZCHECK_EQ((uint64_t)values.size().value(), (uint64_t)3);
  ZCHECK_EQ(values.GET(ULong((uint64_t)0)).value(), 1);
  ZCHECK_EQ(values.GET(ULong((uint64_t)2)).value(), 3);
}
