#include "basetable.h"


namespace Zigurat
{

  std::string BaseTable::name = "Zigurat::BaseTable";
  hashkey_t BaseTable::hash_key = {0xfa, 0x97, 0x2d, 0x28, 0xe0, 0x5b, 0x79, 0xec, 0xaa, 0x4f, 
				   0xcf, 0xe9, 0x9f, 0x73, 0x11, 0x1c, 0x53, 0x96, 0xf3, 0xb7};

  BaseTable::BaseTable()
  {

  }

  BaseTable::BaseTable(const Pointer& pointer)
    : pointer(pointer)
  {

  }

  BaseTable::BaseTable(Pointer&& pointer)
    : pointer(std::move(pointer))
  {

  }

  BaseTable::BaseTable(const BaseTable& other)
  {
    this->pointer = other.pointer;
  }

  BaseTable::BaseTable(BaseTable&& other)
  {
    this->pointer = std::move(other.pointer);
  }

  BaseTable& BaseTable::operator=(const BaseTable& other)
  {
    this->pointer = other.pointer;
    return *this;
  }
    
  BaseTable& BaseTable::operator=(BaseTable&& other)
  {
    this->pointer = std::move(other.pointer);
    return *this;
  }
    
  BaseTable::~BaseTable()
  {
    
  }

}
