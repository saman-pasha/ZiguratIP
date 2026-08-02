#include "tdbyte.h"


namespace Zigurat
{

    const uint8_t TDByte::OBJECT    = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_NULL     | TDByte::TYPEOF_NULL;
    const uint8_t TDByte::BOOL      = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_BYTE;
    const uint8_t TDByte::CHAR      = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_BYTE;
    const uint8_t TDByte::BYTE      = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_BYTE;
    const uint8_t TDByte::UBYTE     = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_UNSIGNED | TDByte::TYPEOF_BYTE;
    const uint8_t TDByte::SHORT     = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_WORD;
    const uint8_t TDByte::USHORT    = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_UNSIGNED | TDByte::TYPEOF_WORD;
    const uint8_t TDByte::INT       = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_DWORD;
    const uint8_t TDByte::UINT      = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_UNSIGNED | TDByte::TYPEOF_DWORD;
    const uint8_t TDByte::LONG      = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_QWORD;
    const uint8_t TDByte::ULONG     = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_UNSIGNED | TDByte::TYPEOF_QWORD;
    const uint8_t TDByte::FLOAT     = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_DECIMAL  | TDByte::TYPEOF_DWORD;
    const uint8_t TDByte::DOUBLE    = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_DECIMAL  | TDByte::TYPEOF_QWORD;
    const uint8_t TDByte::REAL      = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_DECIMAL  | TDByte::TYPEOF_XWORD;
    const uint8_t TDByte::TIMESTAMP = TDByte::SCALEOF_ATOMIC | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_QWORD;
    const uint8_t TDByte::STRING    = TDByte::SCALEOF_BYTE   | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_BYTE;
    const uint8_t TDByte::TEXT      = TDByte::SCALEOF_WORD   | TDByte::MODELOF_SIGNED   | TDByte::TYPEOF_BYTE;
    const uint8_t TDByte::VECTOR    = TDByte::SCALEOF_DWORD;

}
