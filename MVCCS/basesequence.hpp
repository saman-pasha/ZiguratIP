
#ifndef __BASESEQUENCE_HPP__
#define __BASESEQUENCE_HPP__


#include <cstdint>
#include "basetable.hpp"
#include "typelong.hpp"
#include "typestring.hpp"
#include "globals.hpp"
#include "btreeindex.hpp"
#include "sequenceexception.hpp"
#include <functional>
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

    // The one row this sequence keeps, or a refusal.
    //
    // Every operation below is a cursor over a hash key that holds exactly one
    // record, and every one of them used to assume the callback ran. When it
    // did not -- no record, or one this transaction cannot see -- CURRENT,
    // NEXT and BACK returned a default constructed Long, which is null, and
    // SET_CURRENT and RESET quietly did nothing at all.
    //
    // A null sequence value does not stay where it was made. It goes straight
    // into an INSERT, so what surfaces is "not null column 'ID'" against a
    // table with nothing wrong with it, several frames from the sequence that
    // could not answer. Ask here, and say so here.
    static void _with_current(std::function<bool (_Derived&)>);

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
    // Without a store there is nothing to look in and nothing to create, and
    // every caller below is about to dereference this. Saying so beats the
    // segmentation fault, and beats the old behaviour even more: this returned
    // quietly with _initialized still false, so the sequence answered null for
    // the rest of the process.
    if (Globals::memory() == nullptr)
      throw SequenceException("sequence '" + _Derived::NAME.value() + "' has no memory to read");

    // FROM, TO and STEP are statics in the sequence's own shared object, and
    // NEXT does arithmetic with all three. Long::operator+ throws Object's
    // NULL_EXCEPTION -- code 1000, "NULL value" -- if either side is null, so a
    // sequence declared without one of them reported a bare "NULL value" from
    // inside an INSERT in a procedure that had nothing to do with it. Check
    // them once, here, where their names are still to hand.
    if (_Derived::FROM.is_null().value())
      throw SequenceException("sequence '" + _Derived::NAME.value() + "' has no FROM");
    if (_Derived::TO.is_null().value())
      throw SequenceException("sequence '" + _Derived::NAME.value() + "' has no TO");
    if (_Derived::STEP.is_null().value())
      throw SequenceException("sequence '" + _Derived::NAME.value() + "' has no STEP");

    // Its own flag, not _initialized. They mean different things -- one is
    // "this process has checked", the other is "the row is there" -- and using
    // the latch as the search result made a sequence that had just been created
    // indistinguishable from one that had been found.
    bool found = false;
    Globals::memory()->cursor<_Derived>([&] (_Derived&) -> bool {
	found = true;
	return true;
      });

    if (!found) {
      _Derived sequence;
      sequence._current = _Derived::FROM;

      Memory::Streams streams(Globals::memory());
      Globals::memory()->_offline_insert(sequence);
    }

    _Derived::_initialized = true;
  }

  template <typename _Derived>
  void BaseSequence<_Derived>::_with_current(std::function<bool (_Derived&)> callback)
  {
    if (!_Derived::_initialized)
      _Derived::_initialize();

    bool found = false;
    Globals::memory()->cursor<_Derived>([&] (_Derived& current_sequence) -> bool {
	found = true;

	if (current_sequence._current.is_null().value())
	  throw SequenceException("sequence '" + _Derived::NAME.value() + "' holds no value");

	return callback(current_sequence);
      });

    if (!found)
      throw SequenceException("sequence '" + _Derived::NAME.value() + "' has no row: it was never created,"
			      " or this transaction cannot see it");
  }

  template <typename _Derived> 
  Long BaseSequence<_Derived>::CURRENT()
  {
    std::lock_guard<std::mutex> lock(_guard);

    Long current;
    _Derived::_with_current([&] (_Derived& current_sequence) -> bool {
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

      _Derived sequence;
      sequence._current = current;
      _Derived::_with_current([&] (_Derived& current_sequence) -> bool {
	  Memory::Streams streams(Globals::memory());
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

    Long current;
    _Derived::_with_current([&] (_Derived& current_sequence) -> bool {
	if (current_sequence._current + _Derived::STEP > _Derived::TO)
	  throw SequenceException("maximum exceeds");

	current = current_sequence._current;
	_Derived sequence;
        sequence._current = current_sequence._current + _Derived::STEP;
	
	Memory::Streams streams(Globals::memory());
	Globals::memory()->_offline_update(current_sequence, sequence);
	return true;
      });
    return current;
  }

  template <typename _Derived> 
  Long BaseSequence<_Derived>::BACK()
  {
    std::lock_guard<std::mutex> lock(_guard);

    Long current;
    _Derived::_with_current([&] (_Derived& current_sequence) -> bool {
	if (current_sequence._current - _Derived::STEP < _Derived::FROM)
	  throw SequenceException("minimum exceeds");

	current = current_sequence._current;
	_Derived sequence;
        sequence._current = current_sequence._current - _Derived::STEP;
	
	Memory::Streams streams(Globals::memory());
	Globals::memory()->_offline_update(current_sequence, sequence);
	return true;
      });
    return current;
  }

  template <typename _Derived> 
  void BaseSequence<_Derived>::RESET()
  {
    std::lock_guard<std::mutex> lock(_guard);

    _Derived sequence;
    sequence._current = _Derived::FROM;
    _Derived::_with_current([&] (_Derived& current_sequence) -> bool {
	Memory::Streams streams(Globals::memory());
	Globals::memory()->_offline_update(current_sequence, sequence);
	return true;
      });
  }

}

#endif // __BASESEQUENCE_HPP__
