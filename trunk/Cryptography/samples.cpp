
  Polynomial Polynomial::div(const Polynomial& lhs, const Polynomial& rhs, Polynomial* rem)
  {
    const int n = rhs.length();
    const int m = lhs.length() - n;

    if (n == 0) {
      throw ArithmeticException("divide by zero");
    } else if (lhs.length() == 0) {
      if (rem != nullptr) rem->fill(0);
      return Polynomial();
    } else if (lhs < rhs) {
      if (rem != nullptr) *rem = lhs;
      return Polynomial();
    } else if (rhs[n - 1] < Polynomial::BASE / 2) { // Normalization
      Polynomial coef = Polynomial::from_uint64((uint64_t)std::ceil((double)BASE / 2 / rhs[n - 1]));
      Polynomial lhsn = lhs * coef;
      Polynomial rhsn = rhs * coef;
      Polynomial quot = Polynomial::div(lhsn, rhsn, rem);
      if (rem != nullptr) *rem = Polynomial::div(*rem, coef, nullptr);
      return quot;
    }

    Polynomial quot(m + 1, 0);
    Polynomial lhsn = lhs;
    Polynomial rhsn = rhs << m;

    if (lhsn >= rhsn) {
      quot[m] = 0x01;
      lhsn    = lhsn - rhsn;
    }

    uint64_t   qj = 0;
    int        l  = 0;
    Polynomial tmp;

    for (int j = m - 1; j >= 0; j--) {
      
      l = lhsn.length();

      if (l < n + j) {
	continue;
      } else if (l == 1) {
	qj = (uint64_t)lhsn[l - 1] / rhs[n - 1];
      } else {
	qj = (((uint64_t)lhsn[l - 1] * BASE) + lhsn[l - 2]) / rhs[n - 1];
      }
      quot[j] = Utility::min(qj, (uint64_t)BASE - 1);

      rhsn = rhs << j;
      tmp  = rhsn * Polynomial::from_uint64(quot[j]);
      while (lhsn < tmp) {
	quot[j] -= 1;
	tmp      = tmp - rhsn;
      }
      lhsn = lhsn - tmp;

    }

    if (rem != nullptr) *rem = lhsn;
    return quot;
  }


  BigNumber a = 1234567890123, b = 6789012345678;
  
  clock_t begin_time = std::clock();
  
  for (size_t i = 0; i < 2000; i++) {
    a = a * b;
  }
 
  for (size_t i = 0; i < 2000; i++) {
    a = a / b;
  }
  
  clock_t end_time = std::clock();
  std::cout << "BigNumber: " << a << " " << 1000.0 * (end_time - begin_time) / CLOCKS_PER_SEC << " ms" << std::endl;
  

  BigInt2_32 a1 = 1234567890123, b1 = 6789012345678;
  
  begin_time = std::clock();
  
  for (size_t i = 0; i < 2000; i++) {
    a1 = a1 * b1;
  }
  
  for (size_t i = 0; i < 2000; i++) {
    a1 = a1 / b1;
  }
  
  end_time = std::clock();
  std::cout << "BigInt2_32: " << a1.to_string() << " " << 1000.0 * (end_time - begin_time) / CLOCKS_PER_SEC << " ms" << std::endl;

  return 0;

  
	/*
        mod_t v = 1, w = p / 2;
	
	for (int64_t i = N / 4; i > 0; i--) {
	  w = mod_t::div(p_1, mod_t::pow(2, i), v);
	  if (w.is_odd() && v.is_zero()) {
	    v = i;
	    break;
	  }
	}
	*/

  std::string structure = R"V(

sha-1WithRSAEncryption   OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-1(1) 5  }
sha-224WithRSAEncryption OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-1(1) 14 }
sha-256WithRSAEncryption OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-1(1) 11 }
sha-384WithRSAEncryption OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-1(1) 12 }
sha-512WithRSAEncryption OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-1(1) 13 }

Certificate ::= SEQUENCE {
    tbsCertificate     TBSCertificate,
    signatureAlgorithm AlgorithmIdentifier,
    signatureValue     BIT STRING 
}

TBSCertificate ::= SEQUENCE {
    version              EXPLICIT Version DEFAULT v1,
    serialNumber         CertificateSerialNumber,
    signature            AlgorithmIdentifier,
    issuer               Name,
    validity             Validity,
    subject              Name,
    subjectPublicKeyInfo SubjectPublicKeyInfo,
    issuerUniqueID       IMPLICIT UniqueIdentifier OPTIONAL,
                         -- If present, version MUST be v2 or v3
    subjectUniqueID      IMPLICIT UniqueIdentifier OPTIONAL,
                         -- If present, version MUST be v2 or v3
    extensions           EXPLICIT Extensions OPTIONAL
                         -- If present, version MUST be v3
}

Version ::= INTEGER { v1(0), v2(1), v3(2) }

CertificateSerialNumber ::= INTEGER

Validity ::= SEQUENCE {
    notBefore Time,
    notAfter  Time 
}

Time ::= CHOICE {
    utcTime     UTCTime,
    generalTime GeneralizedTime 
}

UniqueIdentifier ::= BIT STRING

SubjectPublicKeyInfo ::= SEQUENCE {
    algorithm        AlgorithmIdentifier,
    subjectPublicKey BIT STRING 
}

Extensions ::= SEQUENCE SIZE (1..MAX) OF Extension

Extension ::= SEQUENCE {
    extnID    OBJECT IDENTIFIER,
    critical  BOOLEAN DEFAULT FALSE,
    extnValue OCTET STRING
              -- contains the DER encoding of an ASN.1 value
              -- corresponding to the extension type identified
              -- by extnID
}

AlgorithmIdentifier ::= SEQUENCE {
    algorithm  OBJECT IDENTIFIER,
    parameters ANY DEFINED BY algorithm OPTIONAL
}

)V";

  Zigurat::ASN1 asn;
  asn.load_struct_string(structure);
  asn.print_structs();

  Zigurat::DER::stream_t stream;
  //Zigurat::DER::encode_length(stream, 150);
  //Zigurat::DER::encode_boolean(stream, true);
  //Zigurat::DER::encode_integer(stream, 12345678);
  //Zigurat::DER::encode_oid(stream, {1, 2, 840, 113549, 1, 1, 5});
  time_t t = std::time(0);
  std::cout << t << std::endl;
  Zigurat::DER::encode_generalized_time(stream, t);

  for (uint8_t octet : stream)
    std::cout << std::hex << (int)octet << ", ";
  std::cout << std::endl;

  Zigurat::DER::position_t cursor = stream.begin();
  //std::cout << std::dec << Zigurat::DER::decode_integer(cursor) << std::endl;
  //Zigurat::DER::oid_t oid = Zigurat::DER::decode_oid(cursor);
  //for (uint32_t node : oid)
  //std::cout << std::dec << node << ", ";
  //std::cout << std::endl;
  std::cout << std::dec << Zigurat::DER::decode_generalized_time(cursor) << std::endl;

  typedef Zigurat::AES128 AES;
  //AES::block_t input = {0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d, 0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34};
  //AES::key_t key = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
  AES::block_t input = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  AES::key_t key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  
  /*
  typedef Zigurat::AES192 AES;
  //AES::key_t key = {0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52, 0xc8, 0x10, 0xf3, 0x2b,
  //		    0x80, 0x90, 0x79, 0xe5, 0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b};
  AES::block_t input = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  AES::key_t key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 
		    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
  */
  /*
  typedef Zigurat::AES256 AES;
  //AES::key_t key = {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
  //		    0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};
  AES::block_t input = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  AES::key_t key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 
		    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 
		    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
  */

  AES::schedule_t exp_key;
  AES::block_t output;
  AES::KeyExpansion(key, exp_key);
  AES::Cipher(input, exp_key, output);
  AES::InverseCipher(output, exp_key, input);

  return 0;

  typedef Zigurat::BigInt2_32 BigInt;
  BigInt aa = {1727393344, 2794025670, 3917761605, 1459772630, 3702157337, 1150707641, 647383926, 847560274, 
  171516770, 1250611412, 2332182731, 4144592295, 3614811596, 1078097445, 3168392856, 3585881937, 1895680571, 483264649, 685214941, 2214780378, 
  682405013, 3607404212, 3182986401, 854613021, 2836473097, 4169779460, 893174185, 1430267905, 536562879, 68795354, 3776782841, 112533914, 
  1082260957, 1929718041, 1074050156, 514483792, 2707997068, 2069706291, 1166675417, 1993758292, 4039242033, 109312347, 2047305523, 2713489267, 
  3357285022, 165107329, 3613557294, 936967221, 2408422965, 3754500070, 258055005, 1700330363, 2055371666, 3729649162, 3019902605, 604525947, 
					       4189607624, 663216775, 828233172, 1046180706, 4031597504, 603237969, 2623944004, 64817549};
  BigInt bb = {87076375, 1035722947, 1161693196, 857662589, 375866530, 1168415369, 4010219141, 3293844903, 
  3879927084, 747750880, 2542741936, 1267415397, 1607409281, 1119640511, 252029414, 3652964884, 225228982, 3620227249, 595563946, 4049855949, 
	       2613490163, 309377264, 535521925, 1085307694, 2148567797, 559859529, 1567219616, 3111891236, 1784146083, 2542028684, 956491426, 694852111};
  
  std::cout << "a:" << aa.to_string() << std::endl;
  std::cout << "b:" << bb.to_string() << std::endl;
  
  std::cout << (aa + bb).to_string() << std::endl;
  std::cout << (aa - bb).to_string() << std::endl;
  std::cout << (aa * bb).to_string() << std::endl; 
  
  BigInt rem;
  BigInt quot = BigInt::div(aa, bb, rem);
  std::cout << "quot: " << quot.to_string() << ", rem: " << rem.to_string() << std::endl;
  
  std::cout << BigInt::mod_pow(0x02, aa, bb).to_string() << std::endl; 
  
  return 0;
  
  typedef Zigurat::BigInt2_32 BigInt;
  BigInt aa = {0x00000000, 0xFFEEDDCC};
  BigInt bb = {0x00000000, 0xCCBBAA00};
  
  std::cout << "a:" << aa.to_string() << std::endl;
  std::cout << "b:" << bb.to_string() << std::endl;
  
  std::cout << (aa + bb).to_string() << std::endl;
  std::cout << (aa - bb).to_string() << std::endl;
  std::cout << (aa * bb).to_string() << std::endl; 
  
  BigInt rem;
  BigInt quot = BigInt::div(aa, bb, rem);
  std::cout << "quot: " << quot.to_string() << ", rem: " << rem.to_string() << std::endl;

  //std::cout << BigInt::pow(aa, bb).to_string() << std::endl; 
  std::cout << BigInt::mod_pow(0x00000002, aa, bb).to_string() << std::endl; 
    
  return 0;

  typedef Zigurat::RSA<1024, Zigurat::SHA::SHA1> RSA;

  int64_t k = 128;
  uint8_t M[16] = {0xd4, 0x36, 0xe9, 0x95, 0x69, 0xfd, 0x32, 0xa7, 0xc8, 0xa0, 0x5b, 0xbc, 0x90, 0xd3, 0x2c, 0x49};
  uint8_t C[k];

  const uint8_t* L = (const uint8_t*)"label";

  typename BigInt p = {0x70BDA6D5, 0x3ECD5335, 0x03435CBC, 0xFC8E9386, 0x871F301E, 0x13975D85, 0xC3B30BDA, 0x0C8C176D, 
		       0x35E2B220, 0xAE65557F, 0x1F1AA014, 0x32C5B78B, 0x62B9FA1C, 0x8DF7DF51, 0x6FF8219E, 0xEC57F765};
  typename BigInt q = {0x5E0B4599, 0x94AC8845, 0x5E654441, 0x903CC4FA, 0x93D9E623, 0x5A83E9A4, 0x669D07FE, 0xF9FA2953, 
		       0x7BCD9704, 0xEE39817B, 0x9CCBB07D, 0x29DD2545, 0xBA0056CF, 0x9CDB366A, 0xD0A6AFCC, 0xE8DBEDCB};
   

   rsa.RSAKG(e, p, q, n, d);

  typename RSA::mod_t n = p * q;
  typename RSA::mod_t e = 65537;
  
  std::cout << "e:" << e.to_string() << std::endl;
  std::cout << "n:" << n.to_string() << std::endl;

  typename RSA::mod_t d, _;
  RSA::EED(e, RSA::LCM(p - 1, q - 1), d, _);

  std::cout << "d:" << d.to_string() << std::endl;

  //RSA::RSAES_OAEP_Encrypt(n, e, M, 16, L, 5, C);
  RSA::RSAES_PKCS1_V1_5_Encrypt(n, e, M, 16, C);

  std::cout << "C: " << std::hex;
  for (size_t i = 0; i < 128; i++)
    std::cout << (int)C[i] << ' ';
  std::cout << std::endl;

  //int64_t mLen = RSA::RSAES_OAEP_Decrypt(n, d, C, L, 5, M);
  int64_t mLen = RSA::RSAES_PKCS1_V1_5_Decrypt(n, d, C, M);

  std::cout << "M: " << std::hex;
  for (int64_t i = 0; i < mLen; i++)
    std::cout << (int)M[i] << ' ';
  std::cout << std::endl;

  std::cout << "Encode: f" << Zigurat::Base16::encode("f") << std::endl;
  std::cout << "Encode: fo" << Zigurat::Base16::encode("fo") << std::endl;
  std::cout << "Encode: foo" << Zigurat::Base16::encode("foo") << std::endl;
  std::cout << "Encode: foob" << Zigurat::Base16::encode("foob") << std::endl;
  std::cout << "Encode: fooba" << Zigurat::Base16::encode("fooba") << std::endl;
  std::cout << "Encode: foobar" << Zigurat::Base16::encode("foobar") << std::endl;
  
  std::cout << "Decode: f" << Zigurat::Base16::decode("66") << std::endl;
  std::cout << "Decode: fo" << Zigurat::Base16::decode("666F") << std::endl;
  std::cout << "Decode: foo" << Zigurat::Base16::decode("666F6F") << std::endl;
  std::cout << "Decode: foob" << Zigurat::Base16::decode("666F6F62") << std::endl;
  std::cout << "Decode: fooba" << Zigurat::Base16::decode("666F6F6261") << std::endl;
  std::cout << "Decode: foobar" << Zigurat::Base16::decode("666F6F626172") << std::endl;

  std::cout << "Encode: f" << Zigurat::Base32::encode("f") << std::endl;
  std::cout << "Encode: fo" << Zigurat::Base32::encode("fo") << std::endl;
  std::cout << "Encode: foo" << Zigurat::Base32::encode("foo") << std::endl;
  std::cout << "Encode: foob" << Zigurat::Base32::encode("foob") << std::endl;
  std::cout << "Encode: fooba" << Zigurat::Base32::encode("fooba") << std::endl;
  std::cout << "Encode: foobar" << Zigurat::Base32::encode("foobar") << std::endl;
  
  std::cout << "Decode: f" << Zigurat::Base32::decode("MY======") << std::endl;
  std::cout << "Decode: fo" << Zigurat::Base32::decode("MZXQ====") << std::endl;
  std::cout << "Decode: foo" << Zigurat::Base32::decode("MZXW6===") << std::endl;
  std::cout << "Decode: foob" << Zigurat::Base32::decode("MZXW6YQ=") << std::endl;
  std::cout << "Decode: fooba" << Zigurat::Base32::decode("MZXW6YTB") << std::endl;
  std::cout << "Decode: foobar" << Zigurat::Base32::decode("MZXW6YTBOI======") << std::endl;
  
  std::cout << "Encode: f" << Zigurat::Base32Hex::encode("f") << std::endl;
  std::cout << "Encode: fo" << Zigurat::Base32Hex::encode("fo") << std::endl;
  std::cout << "Encode: foo" << Zigurat::Base32Hex::encode("foo") << std::endl;
  std::cout << "Encode: foob" << Zigurat::Base32Hex::encode("foob") << std::endl;
  std::cout << "Encode: fooba" << Zigurat::Base32Hex::encode("fooba") << std::endl;
  std::cout << "Encode: foobar" << Zigurat::Base32Hex::encode("foobar") << std::endl;
  
  std::cout << "Decode: f" << Zigurat::Base32Hex::decode("CO======") << std::endl;
  std::cout << "Decode: fo" << Zigurat::Base32Hex::decode("CPNG====") << std::endl;
  std::cout << "Decode: foo" << Zigurat::Base32Hex::decode("CPNMU===") << std::endl;
  std::cout << "Decode: foob" << Zigurat::Base32Hex::decode("CPNMUOG=") << std::endl;
  std::cout << "Decode: fooba" << Zigurat::Base32Hex::decode("CPNMUOJ1") << std::endl;
  std::cout << "Decode: foobar" << Zigurat::Base32Hex::decode("CPNMUOJ1E8======") << std::endl;

  std::cout << "Encode: f" << Zigurat::Base64::encode("f") << std::endl;
  std::cout << "Encode: fo" << Zigurat::Base64::encode("fo") << std::endl;
  std::cout << "Encode: foo" << Zigurat::Base64::encode("foo") << std::endl;
  std::cout << "Encode: foob" << Zigurat::Base64::encode("foob") << std::endl;
  std::cout << "Encode: fooba" << Zigurat::Base64::encode("fooba") << std::endl;
  std::cout << "Encode: foobar" << Zigurat::Base64::encode("foobar") << std::endl;
  
  std::cout << "Decode: f" << Zigurat::Base64::decode("Zg==") << std::endl;
  std::cout << "Decode: fo" << Zigurat::Base64::decode("Zm8=") << std::endl;
  std::cout << "Decode: foo" << Zigurat::Base64::decode("Zm9v") << std::endl;
  std::cout << "Decode: foob" << Zigurat::Base64::decode("Zm9vYg==") << std::endl;
  std::cout << "Decode: fooba" << Zigurat::Base64::decode("Zm9vYmE=") << std::endl;
  std::cout << "Decode: foobar" << Zigurat::Base64::decode("Zm9vYmFy") << std::endl;

  std::cout << "Encode: f" << Zigurat::Base64URL::encode("f") << std::endl;
  std::cout << "Encode: fo" << Zigurat::Base64URL::encode("fo") << std::endl;
  std::cout << "Encode: foo" << Zigurat::Base64URL::encode("foo") << std::endl;
  std::cout << "Encode: foob" << Zigurat::Base64URL::encode("foob") << std::endl;
  std::cout << "Encode: fooba" << Zigurat::Base64URL::encode("fooba") << std::endl;
  std::cout << "Encode: foobar" << Zigurat::Base64URL::encode("foobar") << std::endl;
  
  std::cout << "Decode: f" << Zigurat::Base64URL::decode("Zg==") << std::endl;
  std::cout << "Decode: fo" << Zigurat::Base64URL::decode("Zm8=") << std::endl;
  std::cout << "Decode: foo" << Zigurat::Base64URL::decode("Zm9v") << std::endl;
  std::cout << "Decode: foob" << Zigurat::Base64URL::decode("Zm9vYg==") << std::endl;
  std::cout << "Decode: fooba" << Zigurat::Base64URL::decode("Zm9vYmE=") << std::endl;
  std::cout << "Decode: foobar" << Zigurat::Base64URL::decode("Zm9vYmFy") << std::endl;
