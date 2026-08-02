
#ifndef __AES_HPP__
#define __AES_HPP__

#include <iostream>

namespace Zigurat
{

  template <int Nk, int Nb, int Nr>
  class AES
  {
  protected:
    static const int     WORD_SIZE = 4;
    static const uint8_t SBOX [16][16];
    static const uint8_t Rcon[255];
    static const uint8_t ISBOX[16][16];
    
  public:
    typedef uint8_t word_t    [WORD_SIZE];
    typedef word_t  block_t   [Nb];
    typedef word_t  key_t     [Nk];
    typedef block_t schedule_t[Nr + 1];

  protected:
    static uint8_t XOR(uint8_t, uint8_t);
    static uint8_t DOT(uint8_t, uint8_t);
    static uint8_t xtime(uint8_t, int);
    static void ADD(word_t, word_t, word_t);
    static void MUL(word_t, word_t, word_t);
    static void SubBytes(block_t);
    static void ShiftRows(block_t);
    static void MixColumns(block_t);
    static void AddRoundKey(block_t, block_t);
    static void SubWord(word_t, word_t);
    static void RotWord(word_t, word_t);
    static void InvSubBytes(block_t);
    static void InvShiftRows(block_t);
    static void InvMixColumns(block_t);

  public:
    static void Cipher(block_t, schedule_t, block_t);
    static void Cipher(std::istream&, schedule_t, std::ostream&);
    static void KeyExpansion(key_t, schedule_t);
    static void InverseCipher(block_t, schedule_t, block_t);
    static void InverseCipher(std::istream&, schedule_t, std::ostream&);
  };

  typedef Zigurat::AES<4, 4, 10> AES128;
  typedef Zigurat::AES<6, 4, 12> AES192;
  typedef Zigurat::AES<8, 4, 14> AES256;

}

#endif // __AES_HPP__
