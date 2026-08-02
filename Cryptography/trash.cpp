  template <int Nk, int Nb, int Nr>
  void AES<Nk, Nb, Nr>::ShiftRows(BLOCK s)
  {
    uint8_t tmp;
    for (int r = 1; r < Nb; r++) {
      for (int t = 0; t < r; t++) {
	tmp = s[r][0];
	for (int c = 0; c < AES::WORD_SIZE - 1; c++) {
	  s[r][c] = s[r][c + 1];
	}
	s[r][AES::WORD_SIZE - 1] = tmp;
      }
    }
  }

  template <int Nk, int Nb, int Nr>
  void AES<Nk, Nb, Nr>::MixColumns(BLOCK s)
  {
    BLOCK sp;
    for (int c = 0; c < AES::WORD_SIZE; c++) {
      sp[0][c] = AES::XOR(AES::XOR(AES::XOR(AES::DOT(0x02, s[0][c]), AES::DOT(0x03, s[1][c])), s[2][c]), s[3][c]);
      sp[1][c] = AES::XOR(AES::XOR(AES::XOR(s[0][c], AES::DOT(0x02, s[1][c])), AES::DOT(0x03, s[2][c])), s[3][c]);
      sp[2][c] = AES::XOR(AES::XOR(AES::XOR(s[0][c], s[1][c]), AES::DOT(0x02, s[2][c])), AES::DOT(0x03, s[3][c]));
      sp[3][c] = AES::XOR(AES::XOR(AES::XOR(AES::DOT(0x03, s[0][c]), s[1][c]), s[2][c]), AES::DOT(0x02, s[3][c]));
    }
    std::memmove(s, sp, sizeof(BLOCK));
  }

  template <int Nk, int Nb, int Nr>
  void AES<Nk, Nb, Nr>::MixColumns(BLOCK s)
  {
    BLOCK sp;
    for (int r = 0; r < Nb; r++) {
      sp[r][0] = AES::XOR(AES::XOR(AES::XOR(AES::DOT(0x02, s[r][0]), AES::DOT(0x03, s[r][1])), s[r][2]), s[r][3]);
      sp[r][1] = AES::XOR(AES::XOR(AES::XOR(s[r][0], AES::DOT(0x02, s[r][1])), AES::DOT(0x03, s[r][2])), s[r][3]);
      sp[r][2] = AES::XOR(AES::XOR(AES::XOR(s[r][0], s[r][1]), AES::DOT(0x02, s[r][2])), AES::DOT(0x03, s[r][3]));
      sp[r][3] = AES::XOR(AES::XOR(AES::XOR(AES::DOT(0x03, s[r][0]), s[r][1]), s[r][2]), AES::DOT(0x02, s[r][3]));
    }
    std::memmove(s, sp, sizeof(BLOCK));
  }

