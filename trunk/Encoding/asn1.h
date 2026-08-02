
#ifndef __ASN1_H__
#define __ASN1_H__

#include <string>
#include <vector>
#include <iostream>

namespace Zigurat
{

  class ASN1                                      // Abstract Syntax Notation
  {
  public:
    class Struct                                  // Data Structure
    {
    public:
      Struct();
      Struct(Struct&&);
      Struct(const Struct&);
      Struct& operator=(Struct&&);
      Struct& operator=(const Struct&);
      std::string name;
      bool is_implicit;
      bool is_explicit;
      std::string type;
      std::string default_value;
      bool is_optional;
      std::vector<Struct>* childs;
      std::string value;
      std::string min_size;
      std::string max_size;
      std::string element_type;
      virtual ~Struct();
    };

  protected:
    static const std::string SPACES;
    static const std::string OPERATORS;

    std::vector<Struct> _structs;
    std::string _read_token(std::istream&);
    std::string _read_name_token(std::istream&);
    std::string _read_type_token(std::istream&);
    std::string _read_integer_token(std::istream&);
    void _load_struct_childs(std::istream&, Struct&);
    void _load_struct(std::istream&, std::vector<Struct>&);
    void _print_struct_child(const Struct&, size_t);
    void _print_struct(const Struct&);

  public:
    ASN1() = default;
    void load_struct_file(std::string);
    void load_struct_string(std::string);
    void print_structs();
  };

}

#endif // __ASN1_H__
