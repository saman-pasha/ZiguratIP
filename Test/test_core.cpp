#include "ztest.h"
#include "utility.h"
#include "array.h"
#include "bigint.h"
#include <cstring>

using namespace Zigurat;


ZTEST(Core, utility_string_trim)
{
  ZCHECK_STR(Utility::trim("   padded   "), "padded");
  ZCHECK_STR(Utility::trim_left("   padded   "), "padded   ");
  ZCHECK_STR(Utility::trim_right("   padded   "), "   padded");
  ZCHECK_STR(Utility::trim("xxdataxx", 'x'), "data");
  ZCHECK_STR(Utility::trim(""), "");
  ZCHECK_STR(Utility::trim("     "), "");
}

ZTEST(Core, utility_string_case)
{
  ZCHECK_STR(Utility::to_upper("ZiguratIP"), "ZIGURATIP");
  ZCHECK_STR(Utility::to_lower("ZiguratIP"), "ziguratip");
  ZCHECK_STR(Utility::to_upper(""), "");
}

ZTEST(Core, utility_split)
{
  std::vector<std::string> parts = Utility::split("a,b,c", ',');
  ZCHECK_EQ(parts.size(), (size_t)3);
  if (parts.size() == 3) {
    ZCHECK_STR(parts[0], "a");
    ZCHECK_STR(parts[1], "b");
    ZCHECK_STR(parts[2], "c");
  }

  std::vector<std::string> words = Utility::split("one two  three");
  ZCHECK(words.size() >= 3);
}

ZTEST(Core, utility_pad)
{
  ZCHECK_STR(Utility::pad_left("7", 3, '0'), "007");
  ZCHECK_STR(Utility::pad_right("7", 3, '0'), "700");
  // Already long enough: returned untouched.
  ZCHECK_STR(Utility::pad_left("12345", 3, '0'), "12345");
}

ZTEST(Core, utility_in)
{
  ZCHECK(Utility::in('u', "Zigurat"));
  ZCHECK(!Utility::in('q', "Zigurat"));
}

ZTEST(Core, utility_min_max)
{
  ZCHECK_EQ(Utility::min<int>(3, 9), 3);
  ZCHECK_EQ(Utility::max<int>(3, 9), 9);
  ZCHECK_EQ(Utility::min<int>(4, 4), 4);
}

// Utility::gcd/lcm call T::is_zero(), so they only accept the big number types
// rather than the built-in integers.
ZTEST(Core, utility_gcd_lcm_over_bigint)
{
  ZCHECK(Utility::gcd(BigInt((int64_t)18), BigInt((int64_t)12)) == BigInt((int64_t)6));
  ZCHECK(Utility::lcm(BigInt((int64_t)4), BigInt((int64_t)6)) == BigInt((int64_t)12));
  ZCHECK(BigInt::gcd(BigInt((int64_t)18), BigInt((int64_t)12)) == BigInt((int64_t)6));
}

ZTEST(Core, utility_pow)
{
  ZCHECK_EQ((Utility::pow<int, int>(2, 10)), 1024);
  ZCHECK_EQ((Utility::pow<int, int>(5, 0)), 1);
}

// These six are the wire-format primitives the binary protocol and TLS record
// layer are built on, so every width gets a fixed known answer plus a round trip.
ZTEST(Core, utility_byte_order_16)
{
  const uint16_t host = 0x0102;
  const uint16_t net  = Utility::htons(host);

  uint8_t octets[2];
  std::memcpy(octets, &net, 2);
  ZCHECK_EQ((int)octets[0], 0x01);   // most significant byte first
  ZCHECK_EQ((int)octets[1], 0x02);

  ZCHECK_EQ(Utility::ntohs(net), host);
}

ZTEST(Core, utility_byte_order_32)
{
  const uint32_t host = 0x01020304u;
  const uint32_t net  = Utility::htonl(host);

  uint8_t octets[4];
  std::memcpy(octets, &net, 4);
  for (int i = 0; i < 4; i++) ZCHECK_EQ((int)octets[i], i + 1);

  ZCHECK_EQ(Utility::ntohl(net), host);
}

ZTEST(Core, utility_byte_order_64)
{
  const uint64_t host = 0x0102030405060708ull;
  const uint64_t net  = Utility::htonll(host);

  uint8_t octets[8];
  std::memcpy(octets, &net, 8);
  for (int i = 0; i < 8; i++) ZCHECK_EQ((int)octets[i], i + 1);

  ZCHECK_EQ(Utility::ntohll(net), host);
}

ZTEST(Core, utility_byte_order_roundtrip_edges)
{
  ZCHECK_EQ(Utility::ntohs(Utility::htons(0)), (uint16_t)0);
  ZCHECK_EQ(Utility::ntohs(Utility::htons(0xFFFF)), (uint16_t)0xFFFF);
  ZCHECK_EQ(Utility::ntohl(Utility::htonl(0xFFFFFFFFu)), 0xFFFFFFFFu);
  ZCHECK_EQ(Utility::ntohll(Utility::htonll(0xFFFFFFFFFFFFFFFFull)), 0xFFFFFFFFFFFFFFFFull);
  ZCHECK_EQ(Utility::ntohll(Utility::htonll(1ull)), 1ull);
}

ZTEST(Core, utility_time_roundtrip)
{
  const time_t stamp = 1600000000;  // 2020-09-13 12:26:40 UTC
  std::string text = Utility::time_to_string(stamp);
  ZCHECK(text.size() > 0);
  ZCHECK_EQ(Utility::string_to_time(text), stamp);
}

ZTEST(Core, utility_octet_as_hex)
{
  const uint8_t octets[4] = {0x00, 0x0F, 0xA5, 0xFF};
  std::string hex = Utility::to_lower(Utility::octet_as_hex(octets, 4));
  ZCHECK_STR(hex, "000fa5ff");
}

ZTEST(Core, utility_environment)
{
  ZCHECK(Utility::os_name().size() > 0);
  ZCHECK(Utility::user_home().size() > 0);
  ZCHECK(Utility::generate_id() != 0);
}

ZTEST(Core, bigint_arithmetic)
{
  BigInt a((int64_t)1234567890);
  BigInt b((int64_t)9876543210);

  BigInt sum = a + b;
  BigInt diff = b - a;
  BigInt prod = a * BigInt((int64_t)2);

  ZCHECK(sum > a);
  ZCHECK(diff > BigInt((int64_t)0));
  ZCHECK(prod > a);
  ZCHECK(a == BigInt((int64_t)1234567890));
  ZCHECK(a != b);
  ZCHECK(a < b);
}

ZTEST(Core, bigint_identities)
{
  BigInt a((int64_t)42);
  BigInt zero((int64_t)0);
  BigInt one((int64_t)1);

  ZCHECK(a + zero == a);
  ZCHECK(a * one == a);
  ZCHECK(a - a == zero);
}
