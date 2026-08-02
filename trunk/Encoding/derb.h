
#ifndef __DER_H__
#define __DER_H__

#include <cstdint>
#include <cstddef>
#include <ctime>
#include <vector>
#include <iostream>

namespace Zigurat
{

  class DER                                 // Distinguished Encoding Rules
  {
  protected:
    enum : size_t {
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
    typedef std::vector<uint8_t> stream_t;
    typedef stream_t::iterator position_t;
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
      SET              = 0x31
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
      friend DER;
      friend std::ostream& operator<<(std::ostream&, const Integer&);
      ~Integer();
    };

    static void encode_length(stream_t&, size_t);                         // Encodes a Length to bytes AS DER format
    static void encode_boolean(stream_t&, bool);                          // Encodes a bool value to DER BOOLEAN format
    static void encode_integer(stream_t&, int8_t);                        // Encodes a 8-Bit signed integer to DER INTEGER format
    static void encode_integer(stream_t&, uint8_t);                       // Encodes a 8-Bit unsigned integer to DER INTEGER format
    static void encode_integer(stream_t&, int16_t);                       // Encodes a 16-Bit signed integer to DER INTEGER format
    static void encode_integer(stream_t&, uint16_t);                      // Encodes a 16-Bit unsigned integer to DER INTEGER format
    static void encode_integer(stream_t&, int32_t);                       // Encodes a 32-Bit signed integer to DER INTEGER format
    static void encode_integer(stream_t&, uint32_t);                      // Encodes a 32-Bit unsigned integer to DER INTEGER format
    static void encode_integer(stream_t&, int64_t);                       // Encodes a 64-Bit signed integer to DER INTEGER format
    static void encode_integer(stream_t&, uint64_t);                      // Encodes a 64-Bit unsigned integer to DER INTEGER format
    static void encode_integer(stream_t&, const uint8_t*, size_t);        // Encodes a BigInt Big-Endian array to DER INTEGER format
    static void encode_bit_string(stream_t&, const uint8_t*, size_t);     // Encodes Big-Endian array to DER BIT STRING format
    static void encode_octet_string(stream_t&, const uint8_t*, size_t);   // Encodes octet array to DER OCTET STRING format
    static void encode_null(stream_t&);                                   // Encodes DER NULL format
    static void encode_oid_node(stream_t&, uint32_t);                     // Encodes a node of OID to DER OBJECT IDENTIFIER format
    static void encode_oid(stream_t&, uint32_t*, size_t);                 // Encodes nodes array to DER OBJECT IDENTIFIER format
    static void encode_oid(stream_t&, oid_t);                             // Encodes nodes vector to DER OBJECT IDENTIFIER format
    static void encode_utf8_string(stream_t&, uint8_t*, size_t);          // Encodes UTF-8 array to DER UTF8 STRING format
    static void encode_printable_string(stream_t&, uint8_t*, size_t);     // Encodes name array to DER PRINTABLE STRING format
    static void encode_ia5_string(stream_t&, uint8_t*, size_t);           // Encodes version array to DER IA5 STRING format
    static void encode_utc_time(stream_t&, time_t, bool = true);          // Encodes a time to DER UTC TIME format
    static void encode_generalized_time(stream_t&, time_t, bool = true);  // Encodes a time to DER GENERALIZED TIME format
    static void encode_bmp_string(stream_t&, uint8_t*, size_t);           // Encodes unicode array to DER BMP STRING format
    static void encode_sequence(stream_t&, size_t);                       // Encodes sequence type and length to DER SEQUENCE format
    static void encode_set(stream_t&, size_t);                            // Encodes set type and length to DER SET format

    static uint8_t  decode_tag(position_t&);                              // Decodes TAG byte of TLV DER format
    static size_t   decode_length(position_t&);                           // Decodes LENGTH bytes of TLV DER format
    static bool     decode_boolean(position_t&);                          // Decodes DER BOOLEAN of a bool value
    static Integer  decode_integer(position_t&);                          // Decodes DER INTEGER to integer value
    static size_t   decode_bit_string(position_t&, uint8_t*);             // Decodes DER BIT STRING to Big-Endian array 
    static size_t   decode_octet_string(position_t&, uint8_t*);           // Decodes DER OCTET STRING to octet array 
    static void     decode_null(position_t&);                             // Decodes DER NULL
    static uint32_t decode_oid_node(position_t&);                         // Decodes a node of DER OBJECT IDENTIFIER to integer
    static size_t   decode_oid(position_t&, uint32_t*);                   // Decodes DER OBJECT IDENTIFIER to nodes array
    static oid_t    decode_oid(position_t&);                              // Decodes DER OBJECT IDENTIFIER to nodes vector
    static size_t   decode_utf8_string(position_t&, uint8_t*);            // Decodes DER UTF8 STRING to UTF-8 array 
    static size_t   decode_printable_string(position_t&, uint8_t*);       // Decodes DER PRINTABLE STRING to name array
    static size_t   decode_ia5_string(position_t&, uint8_t*);             // Decodes DER IA5 STRING to version array 
    static time_t   decode_utc_time(position_t&);                         // Decodes DER UTC TIME to a time value 
    static time_t   decode_generalized_time(position_t&);                 // Decodes DER GENERALIZED TIME to a time value
    static size_t   decode_bmp_string(position_t&, uint8_t*);             // Decodes DER BMP STRING to unicode array
    static size_t   decode_sequence(position_t&);                         // Decodes DER SEQUENCE to sequence type and length
    static size_t   decode_set(position_t&);                              // Decodes DER SET to set type and length
  };

}

#endif // __DER_H__
