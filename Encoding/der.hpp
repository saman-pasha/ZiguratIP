
#ifndef __DER_HPP__
#define __DER_HPP__

#include <cstdint>
#include <cstddef>
#include <ctime>
#include <vector>
#include "binarystream.hpp"
#include "bigint.hpp"

namespace Zigurat
{

  class DER                                 // Distinguished Encoding Rules
  {
  protected:
    enum : uint64_t {
      POW_2_7  = 0x80,
      POW_2_8  = 0x100,
      POW_2_15 = 0x8000,
      POW_2_16 = 0x10000,
      POW_2_23 = 0x800000,
      POW_2_24 = 0x1000000,
      POW_2_31 = 0x80000000,
      POW_2_32 = 0x100000000,
      POW_2_39 = 0x8000000000,
      POW_2_40 = 0x10000000000,
      POW_2_47 = 0x800000000000,
      POW_2_48 = 0x1000000000000,
      POW_2_55 = 0x80000000000000,
      POW_2_56 = 0x100000000000000,
      POW_2_63 = 0x8000000000000000
    };

  public:
    typedef std::vector<uint32_t> oid_t;

    enum : uint8_t {
      BOOLEAN          = 0x01,
      INTEGER          = 0x02,
      BIT_STRING       = 0x03,
      OCTET_STRING     = 0x04,
      NULL_VALUE       = 0x05,
      OID              = 0x06,
      UTF8_STRING      = 0x0C,
      PRINTABLE_STRING = 0x13,
      IA5_STRING       = 0x16,
      UTC_TIME         = 0x17,
      GENERALIZED_TIME = 0x18,
      BMP_STRING       = 0x1E,
      SEQUENCE         = 0x30,
      SET              = 0x31,

      // Context specific and constructed: [0] is 0xA0, [3] is 0xA3. X.509 uses
      // these for the optional fields of a v3 certificate -- the version and the
      // extensions -- which is why they are needed at all.
      CONTEXT          = 0xA0
    };

    class Integer
    {
    private:
      uint8_t* _array;
      size_t _length;
      bool _sign;
    public:
      Integer(size_t, bool);
      Integer(const Integer&);
      size_t length() const;
      bool has_sign() const;
      operator int8_t() const;
      operator uint8_t() const;
      operator int16_t() const;
      operator uint16_t() const;
      operator int32_t() const;
      operator uint32_t() const;
      operator int64_t() const;
      operator uint64_t() const;
      operator BigInt() const;
      friend DER;
      friend binarystream& operator<<(binarystream&, const Integer&);
      ~Integer();
    };

  public:
    static void encode_length(binarystream&, uint64_t);                       // Encodes a Length to bytes AS DER format
    static void encode_boolean(binarystream&, bool);                          // Encodes a bool value to DER BOOLEAN format
    static void encode_integer(binarystream&, int8_t);                        // Encodes a 8-Bit signed integer to DER INTEGER format
    static void encode_integer(binarystream&, uint8_t);                       // Encodes a 8-Bit unsigned integer to DER INTEGER format
    static void encode_integer(binarystream&, int16_t);                       // Encodes a 16-Bit signed integer to DER INTEGER format
    static void encode_integer(binarystream&, uint16_t);                      // Encodes a 16-Bit unsigned integer to DER INTEGER format
    static void encode_integer(binarystream&, int32_t);                       // Encodes a 32-Bit signed integer to DER INTEGER format
    static void encode_integer(binarystream&, uint32_t);                      // Encodes a 32-Bit unsigned integer to DER INTEGER format
    static void encode_integer(binarystream&, int64_t);                       // Encodes a 64-Bit signed integer to DER INTEGER format
    static void encode_integer(binarystream&, uint64_t);                      // Encodes a 64-Bit unsigned integer to DER INTEGER format
    static void encode_integer(binarystream&, const uint8_t*, size_t);        // Encodes BigInt Big-Endian array to DER INTEGER format
    static void encode_integer(binarystream&, binarystream&);                 // Encodes BigInt Big-Endian buffer to DER INTEGER format
    static void encode_integer(binarystream&, const BigInt&);                 // Encodes BigInt base 2 ^ 32 to DER INTEGER format
    static void encode_bit_string(binarystream&, const uint8_t*, size_t);     // Encodes Big-Endian array to DER BIT STRING format
    static void encode_bit_string(binarystream&, binarystream&, size_t);      // Encodes Big-Endian binarystream to DER BIT STRING format
    static void encode_octet_string(binarystream&, const uint8_t*, size_t);   // Encodes octet array to DER OCTET STRING format
    static void encode_octet_string(binarystream&, binarystream&);            // Encodes binarystream to to DER OCTET STRING format
    static void encode_null(binarystream&);                                   // Encodes DER NULL format
    static void encode_oid_node(binarystream&, uint32_t);                     // Encodes a node of OID to DER OBJECT IDENTIFIER format
    static void encode_oid(binarystream&, uint32_t*, size_t);                 // Encodes nodes array to DER OBJECT IDENTIFIER format
    static void encode_oid(binarystream&, oid_t);                             // Encodes nodes vector to DER OBJECT IDENTIFIER format
    static void encode_utf8_string(binarystream&, const uint8_t*, size_t);    // Encodes UTF-8 array to DER UTF8 STRING format
    static void encode_utf8_string(binarystream&, const std::string);         // Encodes UTF-8 string to DER UTF8 STRING format
    static void encode_printable_string(binarystream&, const char*, size_t);  // Encodes name array to DER PRINTABLE STRING format
    static void encode_printable_string(binarystream&, const std::string);    // Encodes name array to DER PRINTABLE STRING format
    static void encode_ia5_string(binarystream&, const char*, size_t);        // Encodes version array to DER IA5 STRING format
    static void encode_ia5_string(binarystream&, const std::string);          // Encodes version array to DER IA5 STRING format
    static void encode_utc_time(binarystream&, time_t, bool = true);          // Encodes a time to DER UTC TIME format
    static void encode_generalized_time(binarystream&, time_t, bool = true);  // Encodes a time to DER GENERALIZED TIME format
    static void encode_bmp_string(binarystream&, uint8_t*, size_t);           // Encodes unicode array to DER BMP STRING format
    static void encode_sequence(binarystream&, binarystream&);
    static void encode_context(binarystream&, binarystream&, uint8_t);        // Wraps content in [n] EXPLICIT
    static bool decode_context(binarystream&, binarystream&, uint8_t);        // Unwraps [n] if that is what comes next                // Encodes sequence type, length, value to DER SEQUENCE format
    static void encode_set(binarystream&, binarystream&);                     // Encodes set type, length, value to DER SET format

    static uint8_t  decode_tag(binarystream&);                                // Decodes TAG byte of TLV DER format
    static uint64_t decode_length(binarystream&);                             // Decodes LENGTH bytes of TLV DER format
    static bool     decode_boolean(binarystream&);                            // Decodes DER BOOLEAN of a bool value
    static Integer  decode_integer(binarystream&);                            // Decodes DER INTEGER to integer value
    static size_t   decode_bit_string(binarystream&, binarystream&);          // Decodes DER BIT STRING to Big-Endian binarystream 
    static size_t   decode_octet_string(binarystream&, binarystream&);        // Decodes DER OCTET STRING to octet binarystream 
    static void     decode_null(binarystream&);                               // Decodes DER NULL
    static uint32_t decode_oid_node(binarystream&);                           // Decodes a node of DER OBJECT IDENTIFIER to integer
    static size_t   decode_oid(binarystream&, uint32_t*);                     // Decodes DER OBJECT IDENTIFIER to nodes array
    static oid_t    decode_oid(binarystream&);                                // Decodes DER OBJECT IDENTIFIER to nodes vector
    static size_t   decode_utf8_string(binarystream&, uint8_t*);              // Decodes DER UTF8 STRING to UTF-8 array 
    static size_t   decode_printable_string(binarystream&, char*);            // Decodes DER PRINTABLE STRING to name array
    static size_t   decode_ia5_string(binarystream&, char*);                  // Decodes DER IA5 STRING to version array 
    static time_t   decode_utc_time(binarystream&);                           // Decodes DER UTC TIME to a time value 
    static time_t   decode_generalized_time(binarystream&);                   // Decodes DER GENERALIZED TIME to a time value
    static size_t   decode_bmp_string(binarystream&, uint8_t*);               // Decodes DER BMP STRING to unicode array
    static void     decode_sequence(binarystream&, binarystream&);            // Decodes DER SEQUENCE to content of sequence
    static void     decode_set(binarystream&, binarystream&);                 // Decodes DER SET to content of set

    static void decode_tlv(binarystream&, binarystream&);
    static std::string oid_to_string(oid_t&);
  };

}

#endif // __DER_HPP__
