#include "asn1.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include "encodingexception.hpp"
#include "utility.hpp"
#include <limits>


namespace Zigurat
{

  const std::string ASN1::SPACES = " \t\v\r\n";
  const std::string ASN1::OPERATORS = "{(,.)}";

  ASN1::Struct::Struct()
    : name(""), 
      is_implicit(false),
      is_explicit(false),
      type(""),
      default_value(""),
      is_optional(false),
      childs(nullptr),
      value(""),
      min_size(""),
      max_size(""),
      element_type("")
  {

  }

  ASN1::Struct::Struct(ASN1::Struct&& other)
    : name(other.name), 
      is_implicit(other.is_implicit),
      is_explicit(other.is_explicit),
      type(other.type),
      default_value(other.default_value),
      is_optional(other.is_optional),
      childs(other.childs),
      value(other.value),
      min_size(other.min_size),
      max_size(other.max_size),
      element_type(other.element_type)
  {
    other.childs = nullptr;
  }

  ASN1::Struct::Struct(const ASN1::Struct& other)
    : name(other.name), 
      is_implicit(other.is_implicit),
      is_explicit(other.is_explicit),
      type(other.type),
      default_value(other.default_value),
      is_optional(other.is_optional),
      childs(nullptr),
      value(other.value),
      min_size(other.min_size),
      max_size(other.max_size),
      element_type(other.element_type)
  {
    if (other.childs != nullptr) {
      this->childs = new std::vector<Struct>();
      this->childs->reserve(other.childs->size());
      for (const Struct& def : *other.childs)
	this->childs->push_back(def);
    }
  }

  ASN1::Struct& ASN1::Struct::operator=(ASN1::Struct&& other)
  {
    this->name = other.name;
    this->is_implicit = other.is_implicit;
    this->is_explicit = other.is_explicit;
    this->type = other.type;
    this->default_value = other.default_value;
    this->is_optional = other.is_optional;
    this->childs = other.childs;
    other.childs = nullptr;
    this->value = other.value;
    this->min_size = other.min_size;
    this->max_size = other.max_size;
    this->element_type = other.element_type;
    return *this;
  }

  ASN1::Struct& ASN1::Struct::operator=(const ASN1::Struct& other)
  {
    this->name = other.name;
    this->is_implicit = other.is_implicit;
    this->is_explicit = other.is_explicit;
    this->type = other.type;
    this->default_value = other.default_value;
    this->is_optional = other.is_optional;
    if (other.childs != nullptr) {
      this->childs = new std::vector<Struct>();
      this->childs->reserve(other.childs->size());
      for (const Struct& def : *other.childs)
	this->childs->push_back(def);
    }
    this->value = other.value;
    this->min_size = other.min_size;
    this->max_size = other.max_size;
    this->element_type = other.element_type;
    return *this;
  }

  ASN1::Struct::~Struct()
  {
    if (this->childs != nullptr) delete this->childs;
  }

  std::string ASN1::_read_token(std::istream& file)
  {
    char pch, ch = '\0';
    std::string token;
    
    while (!file.eof()) {
      pch = ch;
      file.get(ch);
      if (pch == '-' && ch == '-') {
	token.pop_back();
	while (!file.eof()) {
	  file.get(ch);
	  if (ch == '\n')
	    break;
	}
	if (token.length() > 0) break;
      } else if (ASN1::SPACES.find(ch) != std::string::npos && token.length() == 0) {
	continue;
      } else if (ASN1::SPACES.find(ch) != std::string::npos && token.length() > 0) {
	break;
      } else if (ASN1::OPERATORS.find(ch) != std::string::npos) {
	if (token.length() == 0)
	  token.push_back(ch);
	else
	  file.seekg(-1, std::ios::cur);
	break;
      } else {
	token.push_back(ch);	
      }
    }

    if (!file.eof() && token.length() == 0) throw EncodingException("ASN.1 invalid token");
    return token;
  }

  std::string ASN1::_read_name_token(std::istream& file)
  {
    std::string token = this->_read_token(file);
    if (token.length() == 0) return token;
    if (token[0] < 'A' || (token[0] > 'Z' && token[0] < 'a') || token[0] > 'z') throw EncodingException("ASN.1 invalid name " + token);
    for (size_t i = 1; i < token.size(); i++) {
      if (token[i] < '0' || (token[i] > '9' && token[i] < 'A') || (token[i] > 'Z' && token[i] < 'a') || token[i] > 'z')
	throw EncodingException("ASN.1 invalid name " + token);
    }
    return token;
  }

  std::string ASN1::_read_type_token(std::istream& file)
  {
    std::string token = this->_read_name_token(file);
    if (token == "BIT" || token == "OCTET") {
      std::string ext = this->_read_name_token(file);
      if (ext != "STRING") throw EncodingException("ASN.1 invalid type " + token + " " + ext);
      return token + " " + ext; 
    } else if (token == "OBJECT") {
      std::string ext = this->_read_name_token(file);
      if (ext != "IDENTIFIER") throw EncodingException("ASN.1 invalid type " + token + " " + ext);
      return token + " " + ext; 
    } else {
      return token;
    }
  }

  std::string ASN1::_read_integer_token(std::istream& file)
  {
    std::string token = this->_read_token(file);
    if (token == "MIN" || token == "MAX")
      return token;
    for (size_t i = 0; i < token.size(); i++) {
      if (token[i] < '0' || token[i] > '9')
	throw EncodingException("ASN.1 invalid integer " + token);
    }
    return token;
  }

  void ASN1::_load_struct_childs(std::istream& file, Struct& p_def)
  {
    std::streampos pos = file.tellg();
    std::string token = this->_read_token(file);
    if (token == "SIZE") {
      token = this->_read_token(file);
      if (token != "(") throw EncodingException("ASN.1 missing '(' at item " + p_def.name);	
      p_def.min_size = this->_read_integer_token(file);
      token = this->_read_token(file);
      if (token == ".") {
	token = this->_read_token(file);
	if (token != ".") throw EncodingException("ASN.1 missing '..' at item " + p_def.name);
	p_def.max_size = this->_read_integer_token(file);
	token = this->_read_token(file);
	if (token != ")") throw EncodingException("ASN.1 missing ')' at item " + p_def.name);
      } else if (token == ")") {
	p_def.min_size = p_def.max_size;
      } else {
	if (token != ")") throw EncodingException("ASN.1 missing ')' at item " + p_def.name);
      }
      token = this->_read_token(file);
      if (token != "OF") throw EncodingException("ASN.1 missing 'OF' at item " + p_def.name);
      p_def.element_type = this->_read_type_token(file);
      return;
    } else if (token == "OF") {
      p_def.element_type = this->_read_type_token(file);
      return;
    } else if (token != "{") {
      file.seekg(pos, std::ios::beg);
      return;
    }
    
    p_def.childs = new std::vector<Struct>();
    while (true) {
    
      ASN1::Struct def;
      def.name = this->_read_name_token(file);

      if (p_def.type == "INTEGER") {
	token = this->_read_token(file);
	if (token != "(") throw EncodingException("ASN.1 missing '(' inside item " + p_def.name + " at " + def.name);	
	def.value = this->_read_integer_token(file);
	token = this->_read_token(file);
	if (token != ")") throw EncodingException("ASN.1 missing ')' inside item " + p_def.name + " at " + def.name);
	token = this->_read_token(file);
      } else {
	token = this->_read_type_token(file);
	if (token == "IMPLICIT") {
	  def.is_implicit = true;
	  def.type = this->_read_type_token(file);
	} else if (token == "EXPLICIT") {
	  def.is_explicit = true;
	  def.type = this->_read_type_token(file);
	} else {
	  def.type = token;
	}
	token = this->_read_token(file);
	if (token == "DEFAULT") {
	  def.default_value = this->_read_token(file);
	  token = this->_read_token(file);
	} else if (token == "OPTIONAL") {
	  def.is_optional = true;
	  token = this->_read_token(file);
	}
      }

      if (token == ",") {
	p_def.childs->push_back(def);
	continue;
      } else if (token.length() == 0 || token == "}") {
	p_def.childs->push_back(def);
	break;
      } else {
	throw EncodingException("ASN.1 invalid syntax inside item " + p_def.name + " at " + def.name);
      }
          
    }
  }

  void ASN1::_load_struct(std::istream& file, std::vector<Struct>& structs)
  {
    std::string token;
    while ((token = this->_read_name_token(file)).length() > 0) {
	
      ASN1::Struct def;
      def.name = token;
      token = this->_read_token(file);
      if (token != "::=") throw EncodingException("ASN.1 invalid item " + def.name);
      def.type = this->_read_type_token(file);
      this->_load_struct_childs(file, def);
      structs.push_back(def);

    }
  }

  void ASN1::load_struct_file(std::string file_path)
  {
    std::ifstream file(file_path);
    if (file.good()) {
      this->_load_struct(file, this->_structs);
    } else {
      throw EncodingException("ASN.1 invalid data structure file");
    }
  }

  void ASN1::load_struct_string(std::string structure)
  {
    std::stringstream file(structure);
    this->_load_struct(file, this->_structs);
  }

  void ASN1::_print_struct_child(const Struct& def, size_t pad)
  {
    std::cout << '\t' << def.name << std::string(pad + 1, ' ');
    if (def.is_implicit) std::cout << "IMPLICIT ";
    if (def.is_explicit) std::cout << "EXPLICIT ";
    if (def.type.length() > 0) std::cout << def.type;
    if (def.value.length() > 0) std::cout << "(" << def.value << ")";
    if (def.default_value.length() > 0) std::cout << " DEFAULT " << def.default_value;
    if (def.is_optional) std::cout << " OPTIONAL";
  }

  void ASN1::_print_struct(const Struct& def)
  {
    std::cout << def.name << " ::= " << def.type;
    if (def.element_type.length() > 0) {
      std::cout << " SIZE(";
      if (def.min_size == def.max_size)
	std::cout << def.min_size;
      else
	std::cout << def.min_size << ".." << def.max_size; 
      std::cout << ") OF " << def.element_type;      
    } else if (def.childs != nullptr) {

      size_t max = 0;
      for (const Struct& child : *def.childs)
        max = Utility::max(max, child.name.size());

      std::cout << " {" << std::endl;
      for (size_t i = 0; i < def.childs->size() - 1; i++) {
	this->_print_struct_child((*def.childs)[i], max - (*def.childs)[i].name.size());
	std::cout << "," << std::endl;
      }
      this->_print_struct_child(def.childs->back(), max - def.childs->back().name.size());
      std::cout << std::endl;
      std::cout << "}";
 
   }
    std::cout << std::endl << std::endl;
  }

  void ASN1::print_structs()
  {
    for (const Struct& def : this->_structs)
      this->_print_struct(def);
  }

}
