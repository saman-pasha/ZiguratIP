
#ifndef __BASESEQUENCE_HPP__
#define __BASESEQUENCE_HPP__


#include <cstdint>
#include "basetable.hpp"
#include "typelong.hpp"
#include "typestring.hpp"
#include "globals.hpp"
#include "btreeindex.hpp"
#include "sequenceexception.hpp"
#include <mutex>

namespace Zigurat
{

  template <typename _Derived> 
  class BaseSequence : public BaseTable
  {
  private:
    static std::mutex _guard;

  public:
    static const String NAME;
    static const Long FROM;
    static const Long TO;
    static const Long STEP;
    static bool _initialized;
    static void _initialize();

    Long _current;

    using BaseTable::BaseTable;
    static hashkey_t hash_key;
    BaseSequence() = default;

    static Long CURRENT();
    static void SET_CURRENT(Long);
    static Long NEXT();
    static Long BACK();
    static void RESET();

    void prepare() override;
    void map() override;
    void unmap() override;
    int64_t pack_size() override;
    template <typename T> friend binarystream& operator<<(binarystream&, const BaseSequence<T>&);
    template <typename T> friend binarystream& operator>>(binarystream&, BaseSequence<T>&);

  };

  template <typename _Derived> 
  std::mutex BaseSequence<_Derived>::_guard;

  template <typename _Derived> 
  bool BaseSequence<_Derived>::_initialized = false;

  template <typename _Derived> 
  void BaseSequence<_Derived>::prepare()
  {

  }

  template <typename _Derived> 
  void BaseSequence<_Derived>::map()
  {

  }

  template <typename _Derived> 
  void BaseSequence<_Derived>::unmap()
  {

  }

  template <typename _Derived> 
  int64_t BaseSequence<_Derived>::pack_size()
  {
    return binarystream::pack_size(this->_current);
  }

  template <typename T>
  binarystream& operator<<(binarystream& ostream, const BaseSequence<T>& sequence)
  {
    ostream.pack(sequence._current);
    return ostream;
  }

  template <typename T>
  binarystream& operator>>(binarystream& istream, BaseSequence<T>& sequence)
  {
    istream.unpack(sequence._current);
    return istream;
  }

  template <typename _Derived> 
  void BaseSequence<_Derived>::_initialize()
  {
    if (Globals::memory()) {
      Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	  _Derived::_initialized = true;
	  return true;
	});
      if (!_Derived::_initialized) {
        _Derived sequence;
        sequence._current = _Derived::FROM;
	
	std::lock_guard<std::mutex> hexmap_lock(Globals::memory()->_hexmap_access);
	std::lock_guard<std::mutex> data_lock(Globals::memory()->_data_access);
	Globals::memory()->_offline_insert(sequence);

	_Derived::_initialized = true;
      }
    }
  }

  template <typename _Derived> 
  Long BaseSequence<_Derived>::CURRENT()
  {
    std::lock_guard<std::mutex> lock(_guard);

    if (!_Derived::_initialized)
      _Derived::_initialize();

    Long current;
    Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	current = current_sequence._current;
	return true;
      });
    return current;
  }

  template <typename _Derived> 
  void BaseSequence<_Derived>::SET_CURRENT(Long current)
  {
    std::lock_guard<std::mutex> lock(_guard);

    if (current >= _Derived::FROM && current <= _Derived::TO) {
      if (!_Derived::_initialized)
	_Derived::_initialize();
      
      _Derived sequence;
      sequence._current = current;
      Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	  std::lock_guard<std::mutex> hexmap_lock(Globals::memory()->_hexmap_access);
	  std::lock_guard<std::mutex> data_lock(Globals::memory()->_data_access);
	  Globals::memory()->_offline_update(current_sequence, sequence);
	  return true;
	});
    } else {
      throw SequenceException("not in range");
    }
  }

  template <typename _Derived> 
  Long BaseSequence<_Derived>::NEXT()
  {
    std::lock_guard<std::mutex> lock(_guard);

    if (!_Derived::_initialized)
      _Derived::_initialize();

    Long current;
    Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	if (current_sequence._current + _Derived::STEP > _Derived::TO)
	  throw SequenceException("maximum exceeds");

	current = current_sequence._current;
	_Derived sequence;
        sequence._current = current_sequence._current + _Derived::STEP;
	
	std::lock_guard<std::mutex> hexmap_lock(Globals::memory()->_hexmap_access);
	std::lock_guard<std::mutex> data_lock(Globals::memory()->_data_access);
	Globals::memory()->_offline_update(current_sequence, sequence);
	return true;
      });
    return current;
  }

  template <typename _Derived> 
  Long BaseSequence<_Derived>::BACK()
  {
    std::lock_guard<std::mutex> lock(_guard);

    if (!_Derived::_initialized)
      _Derived::_initialize();

    Long current;
    Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	if (current_sequence._current - _Derived::STEP < _Derived::FROM)
	  throw SequenceException("minimum exceeds");

	current = current_sequence._current;
	_Derived sequence;
        sequence._current = current_sequence._current - _Derived::STEP;
	
	std::lock_guard<std::mutex> hexmap_lock(Globals::memory()->_hexmap_access);
	std::lock_guard<std::mutex> data_lock(Globals::memory()->_data_access);
	Globals::memory()->_offline_update(current_sequence, sequence);
	return true;
      });
    return current;
  }

  template <typename _Derived> 
  void BaseSequence<_Derived>::RESET()
  {
    std::lock_guard<std::mutex> lock(_guard);

    if (!_Derived::_initialized)
      _Derived::_initialize();

    _Derived sequence;
    sequence._current = _Derived::FROM;
    Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	std::lock_guard<std::mutex> hexmap_lock(Globals::memory()->_hexmap_access);
	std::lock_guard<std::mutex> data_lock(Globals::memory()->_data_access);
	Globals::memory()->_offline_update(current_sequence, sequence);
	return true;
      });
  }

}

#endif // __BASESEQUENCE_HPP__
