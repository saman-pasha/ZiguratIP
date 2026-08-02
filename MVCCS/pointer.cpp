#include "pointer.hpp"
#include <memory>


namespace Zigurat
{

  Pointer::Pointer()
    : hash_key(nullptr), address(0), size(0) 
  {

  }

  Pointer::Pointer(hashkey_ptr hash_key, int64_t address, int64_t size)
    : hash_key(hash_key), address(address), size(size) 
  { 

  }

  Pointer::Pointer(const Pointer& other)
    : hash_key(other.hash_key), address(other.address), size(other.size)
  {
    
  }
   
  Pointer::Pointer(Pointer&& other)
    : hash_key(std::move(other.hash_key)), address(std::move(other.address)), size(std::move(other.size))
  {

  }
    
  Pointer& Pointer::operator=(const Pointer& other)
  {
    this->hash_key = other.hash_key;
    this->address = other.address;
    this->size = other.size;
    return *this;
  }

  Pointer& Pointer::operator=(Pointer&& other)
  {
    this->hash_key = std::move(other.hash_key);
    this->address = std::move(other.address);
    this->size = std::move(other.size);
    return *this;
  }

  Pointer::~Pointer()
  {
    
  }

}
