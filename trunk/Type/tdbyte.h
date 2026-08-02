
#ifndef __TDBYTE_H__
#define __TDBYTE_H__

#include <cstdint>
#include <cmath>
#include "utility.h"

namespace Zigurat
{

  // Type Descriptor Byte
  class TDByte
  {
  public:
    static const uint8_t IS_NULL          = 0x80;

    static const uint8_t SCALEOF_ATOMIC   = 0x00;
    static const uint8_t SCALEOF_BYTE     = 0x20;
    static const uint8_t SCALEOF_WORD     = 0x40;
    static const uint8_t SCALEOF_DWORD    = 0x60;
    
    static const uint8_t MODELOF_NULL     = 0x00;
    static const uint8_t MODELOF_SIGNED   = 0x08;
    static const uint8_t MODELOF_UNSIGNED = 0x10;
    static const uint8_t MODELOF_DECIMAL  = 0x18;
    
    static const uint8_t TYPEOF_NULL      = 0x00;
    static const uint8_t TYPEOF_BYTE      = 0x01;
    static const uint8_t TYPEOF_WORD      = 0x02;
    static const uint8_t TYPEOF_DWORD     = 0x03;
    static const uint8_t TYPEOF_QWORD     = 0x04;
    static const uint8_t TYPEOF_XWORD     = 0x05;
    static const uint8_t TYPEOF_YWORD     = 0x06;
    static const uint8_t TYPEOF_ZWORD     = 0x07;

    static const uint8_t SIZEOF_NULL      = 0x00;
    static const uint8_t SIZEOF_BYTE      = 0x01;
    static const uint8_t SIZEOF_WORD      = 0x02;
    static const uint8_t SIZEOF_DWORD     = 0x04;
    static const uint8_t SIZEOF_QWORD     = 0x08;
    static const uint8_t SIZEOF_XWORD     = 0x10;
    static const uint8_t SIZEOF_YWORD     = 0x20;
    static const uint8_t SIZEOF_ZWORD     = 0x40;

    static constexpr int64_t SCALEOF(uint8_t);
    static constexpr int64_t MODELOF(uint8_t);
    static constexpr int64_t TYPEOF(uint8_t);
    static constexpr int64_t SIZEOF(uint8_t);

    static const uint8_t OBJECT;
    static const uint8_t BOOL;
    static const uint8_t CHAR;
    static const uint8_t BYTE;
    static const uint8_t UBYTE;
    static const uint8_t SHORT;
    static const uint8_t USHORT;
    static const uint8_t INT;
    static const uint8_t UINT;
    static const uint8_t LONG;
    static const uint8_t ULONG;
    static const uint8_t FLOAT;
    static const uint8_t DOUBLE;
    static const uint8_t REAL;
    static const uint8_t TIMESTAMP;
    static const uint8_t STRING;
    static const uint8_t TEXT;
    static const uint8_t VECTOR;

  };

  constexpr int64_t TDByte::SCALEOF(uint8_t tdb)
  {
    return (tdb & TDByte::SCALEOF_DWORD);
  }

  constexpr int64_t TDByte::MODELOF(uint8_t tdb)
  {
    return (tdb & TDByte::MODELOF_DECIMAL);
  }
  constexpr int64_t TDByte::TYPEOF(uint8_t tdb)
  {
    return (tdb & TDByte::TYPEOF_ZWORD);
  }

  constexpr int64_t TDByte::SIZEOF(uint8_t tdb)
  {
    return Utility::pow(2, (tdb & TDByte::TYPEOF_ZWORD) - 1);
  }
  
}

#endif // __TDBYTE_H__
