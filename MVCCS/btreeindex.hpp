
#ifndef __BTREEINDEX_HPP__
#define __BTREEINDEX_HPP__


#include <cstdint>
#include "memory.hpp"
#include "btreerecord.hpp"
#include "btreenode.hpp"
#include "btreekey.hpp"
#include "btreevalue.hpp"
#include "indexexception.hpp"
#include "control.hpp"
#include "shahelper.hpp"
#include "typeint.hpp"
#include "typelong.hpp"
#include "utility.hpp"
#include <tuple>
#include <vector>
#include <memory>
#include <functional>

namespace Zigurat
{

  template <typename ..._Args>
  class BTreeIndex;
  
  template <typename _Table, typename _First>
  class BTreeIndex<_Table, _First>
  {
  protected:
    Memory* _memory;
    const std::string _name;
    const bool _is_unique;
    const int16_t _branching_factor; // d + 1
    const int16_t _min_degree; // d
    const int16_t _max_degree; // 2d
    _First _Table::* _column;
    Pointer _pointer;
    Long _root_address;
    hashkey_t _hash_key;
    std::string _hash_name;
    bool _is_dependent = false;
    bool _is_multi_level = false;

    // The catalogue index, which is the one index that cannot be treated like
    // the others: it cannot look itself up through the catalogue, and it cannot
    // be written inside a transaction. Both follow from the same fact, so it is
    // decided once here rather than compared by name in two places.
    bool _is_catalogue = false;

    static bool is_catalogue_name(const std::string& name)
    {
      return (name == "IDX_ZIGURAT_BTREERECORD_HASH_NAME");
    }

    void _insert_btreeindex();
    void _select_btreeindex();
    void _update_btreeindex();
    
    void _insert_btreevalue(BTreeKey<_First>&, const Long&);
    void _delete_btreevalue(BTreeValue&);

    BTreeNode _btreenode(const Long&);
    BTreeKey<_First> _btreekey(const Long&);

    // The in-order successor's home: the leftmost leaf key of a subtree.
    BTreeKey<_First> _leftmost_key(const Long&);

    void _cursor_keys(BTreeNode&, std::function<bool (int16_t, BTreeKey<_First>&)>&&);
    void _cursor_values(const BTreeKey<_First>&, std::function<bool (int64_t, BTreeValue&)>&&);

    // The chain of nodes above the one being worked on, root first. A split
    // promotes a key into its parent, which can push the parent over the limit
    // and split that too, all the way up -- so the whole path has to be to hand,
    // not just one level of it.
    typedef std::vector<BTreeNode*> path_t;

    void _split_node(path_t&, BTreeNode&);
    void _map_to_internal(path_t&, BTreeNode&, BTreeKey<_First>&);
    virtual void _map_callback(const _Table&, const _First&, const Long&, BTreeKey<_First>&);
    void _map(path_t&, BTreeNode&, const _Table&, const _First&, const Long&);

    virtual void _unmap_callback(const _Table&, const _First&, const Long&, BTreeKey<_First>&);
    void _unmap(BTreeNode&, const _Table&, const _First&, const Long&);

    void _free_key_values(BTreeKey<_First>&);
    void _unlink_dead_values(BTreeKey<_First>&);

    // Rebuild every chain in this index without its settled dead links. Virtual
    // because a composite index keeps its values a level down, so its own keys
    // have none and the walk has to carry on into the dependents.
    virtual void _unlink_dead();
    void _combine_nodes(BTreeNode&, BTreeKey<_First>&);
    virtual void _unmap_key_callback(const _Table&, const _First&, BTreeKey<_First>&);
    void _unmap_key(BTreeNode*, BTreeKey<_First>*, BTreeNode&, const _Table&, const _First&);
  
    bool _cursor_output(const BTreeKey<_First>&, const std::function<bool (_Table&)>&);
    
    // Each returns false once the caller's callback has asked to stop, so the
    // recursion unwinds instead of carrying on in the parent node.
    bool _cursor(const Long&, const std::function<bool (BTreeKey<_First>&)>&);
    bool _cursor_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    bool _cursor_not_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    bool _cursor_less_than(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    bool _cursor_less_than_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    bool _cursor_greater_than(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    bool _cursor_greater_than_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);

  public:
    BTreeIndex(Memory*, std::string, bool, _First _Table::*);
    BTreeIndex(Memory*, std::string, bool, std::string, Long, std::tuple<_First _Table::*>);

    void set_memory(Memory*);
    Long root_address();

    void map(const _Table&);
    void unmap(const _Table&);
    void unmap_key(const _Table&);

    // Gives back the space of every settled dead entry, the way
    // Memory::truncate does for a table's rows. Deleting a row takes its index
    // entry with it, so truncating a table without truncating its indexes hands
    // back the rows and keeps the entries that pointed at them.
    size_t truncate();

    void cursor(std::function<bool (_Table&)>);
    void cursor_equal(const _First, std::function<bool (_Table&)>);
    void cursor_not_equal(const _First, std::function<bool (_Table&)>);
    void cursor_less_than(const _First, std::function<bool (_Table&)>);
    void cursor_less_than_equal(const _First, std::function<bool (_Table&)>);
    void cursor_greater_than(const _First, std::function<bool (_Table&)>);
    void cursor_greater_than_equal(const _First, std::function<bool (_Table&)>);

    // The multi column index derives from this one and overrides the map and
    // unmap callbacks, so it has to be destroyable through a base pointer.
    virtual ~BTreeIndex() { }

    // A composite index builds the index one level down and works through it,
    // which is reaching into a different specialisation rather than into its
    // own base -- protected access alone does not reach that far.
    template <typename...> friend class BTreeIndex;
  };

  template <typename _Table, typename _First>
  BTreeIndex<_Table, _First>::BTreeIndex(Memory* memory, std::string name, bool is_unique,
					 _First _Table::* column)
    : _memory(memory),
      _name(name),
      _is_unique(is_unique),
      _branching_factor( (TDByte::SIZEOF(_First::TDB) * 8) + 1),
      _min_degree(_branching_factor - 1),
      _max_degree(_min_degree * 2),
      _column(column),
      _root_address(-1)
  {
    if (this->_memory && this->_memory->_watcher) {
      this->_memory->dba_watch("Independent BTreeIndex: (" + this->_name + ")");
    }    

    this->_is_catalogue = BTreeIndex<_Table, _First>::is_catalogue_name(this->_name);

    SHA::checksum(SHA::SHA1, (const uint8_t*)this->_name.c_str(), this->_name.size(), this->_hash_key);
    this->_hash_name = Utility::octet_as_hex(this->_hash_key, Memory::HASHKEY_SIZE);

    if (this->_memory)
      this->_select_btreeindex();
  }

  template <typename _Table, typename _First>
  BTreeIndex<_Table, _First>::BTreeIndex(Memory* memory, std::string name, bool is_unique,
					 std::string hash_name, Long root_address, 
					 std::tuple<_First _Table::*> column) 
    : _memory(memory),
      _name(name),
      _is_unique(is_unique),
      _branching_factor( (TDByte::SIZEOF(_First::TDB) * 8) + 1),
      _min_degree(_branching_factor - 1),
      _max_degree(_min_degree * 2),
      _column(std::get<0>(column)),
      _root_address(root_address)
  {
    if (this->_memory && this->_memory->_watcher) {
      this->_memory->dba_watch("Dependent BTreeIndex: (" + this->_name + ")");
    }    

    this->_is_dependent = true;
    this->_is_catalogue = BTreeIndex<_Table, _First>::is_catalogue_name(this->_name);
    this->_hash_name = hash_name;
    std::memcpy(this->_hash_key, this->_hash_name.c_str(), Memory::HASHKEY_SIZE);
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::set_memory(Memory* memory)
  {
    this->_memory = memory;
    if (this->_memory)
      this->_select_btreeindex();
  }

  template <typename _Table, typename _First>
  Long BTreeIndex<_Table, _First>::root_address()
  {
    return this->_root_address;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_insert_btreeindex()
  {
    Memory::Streams streams(this->_memory);

    String hash_name_(this->_hash_name);
    Bool is_unique_(this->_is_unique);
    Short branching_factor_(this->_branching_factor);
    Long root_address_(this->_root_address);

    int64_t size = binarystream::pack_size(hash_name_, is_unique_, branching_factor_, root_address_);
    this->_pointer = this->_memory->_allocate(Memory::INDICES_HASHKEY, size);
    
    Control control;
    control.offline_state = RowState::INSERTED;
    control.create_time = Memory::version_time();
    this->_memory->_dump_control(this->_pointer, control);

    this->_memory->_full_hexmap(this->_pointer);
    this->_memory->_data_io.seekp(this->_memory->_pointer_data_address(this->_pointer), std::ios::beg);
    this->_memory->_data_io.pack(hash_name_, is_unique_, branching_factor_, root_address_);

    if (BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME) {
      BTreeRecord record;
      record.*BTreeRecord::hash_name = this->_hash_name;
      record.*BTreeRecord::is_unique = this->_is_unique;
      record.*BTreeRecord::branching_factor = this->_branching_factor;
      record.*BTreeRecord::root_address = this->_root_address;
      record.pointer = this->_pointer;
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME->map(record);
    }

    this->_memory->_hexmap_io.flush();
    this->_memory->_data_io.flush();
  }
  
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_select_btreeindex()
  {
    // The catalogue index cannot look itself up through the catalogue, so it
    // scans the index rows directly. Not recording the hit there left the
    // catalogue re-inserting its own row on every restart: the root address was
    // read back correctly, but the store gained a duplicate each time.
    bool found = false;
    if (this->_is_catalogue) {

      Memory::Streams streams(this->_memory);

      this->_memory->_cursor(Memory::INDICES_HASHKEY, &streams, [&] (const Pointer& index_pointer) -> bool {

	  String hash_name_;
	  Bool is_unique_;
	  Short branching_factor_;
	  Long root_address_;

	  // _cursor hands the streams back before calling this, so reading the
	  // index row out has to take them again.
	  Memory::Streams row(this->_memory);

	  this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(index_pointer), std::ios::beg);
	  this->_memory->_data_io.unpack(hash_name_, is_unique_, branching_factor_, root_address_);

	  if (this->_hash_name == hash_name_.value()) {
	    this->_root_address = root_address_;
	    this->_pointer = index_pointer;
	    found = true;
	    return false;
	  }

	  return true;
	});
    } else {
      // Every other index is looked up through the catalogue index, so that one
      // has to exist first. Saying so beats dereferencing a null static.
      if (BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME == nullptr)
	throw IndexException("IDX_ZIGURAT_BTREERECORD_HASH_NAME must be created before '" + this->_name + "'");

      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME->cursor_equal(this->_hash_name, [&] (BTreeRecord& record) -> bool {

	  this->_root_address = record.*BTreeRecord::root_address;
	  this->_pointer = record.pointer;

	  found = true;
	  return false;
	});
    }

    if (!found) {    
      this->_insert_btreeindex();
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_update_btreeindex()
  {
    Memory::Streams streams(this->_memory);

    String hash_name_(this->_hash_name);
    Bool is_unique_(this->_is_unique);
    Short branching_factor_(this->_branching_factor);
    Long root_address_(this->_root_address);

    Control control;
    this->_memory->_load_control(this->_pointer, control);
    control.modify_time = Memory::version_time();
    this->_memory->_dump_control(this->_pointer, control);

    this->_memory->_data_io.seekp(this->_memory->_pointer_data_address(this->_pointer), std::ios::beg);
    this->_memory->_data_io.pack(hash_name_, is_unique_, branching_factor_, root_address_);

    this->_memory->_hexmap_io.flush();
    this->_memory->_data_io.flush();
  }
  
  template <typename _Table, typename _First>
  BTreeNode BTreeIndex<_Table, _First>::_btreenode(const Long& address)
  {
    BTreeNode btreenode;
    btreenode.pointer = this->_memory->_pointer(this->_hash_key, address);
    this->_memory->_offline_select(btreenode);
    return btreenode;
  }
    
  template <typename _Table, typename _First>
  BTreeKey<_First> BTreeIndex<_Table, _First>::_btreekey(const Long& address)
  {
    BTreeKey<_First> btreekey;
    btreekey.pointer = this->_memory->_pointer(this->_hash_key, address);
    this->_memory->_offline_select(btreekey);
    return btreekey;
  }

  template <typename _Table, typename _First>
  BTreeKey<_First> BTreeIndex<_Table, _First>::_leftmost_key(const Long& node_address)
  {
    BTreeNode node = this->_btreenode(node_address);
    BTreeKey<_First> first = this->_btreekey(node.keys_address);
    if (node.is_internal)
      return this->_leftmost_key(first.left_node_address);
    return first;
  }

  // Allocated under the index's own hash key, beside the nodes and keys that
  // were always there, and linked at the head of the key's chain. The head is
  // the cheap end: pushing there is two writes and never walks what is already
  // in the chain.
  //
  // The key is written back here rather than by the caller. Every path that
  // reaches this has just inserted or updated the key record, so it exists, and
  // leaving the new head to be persisted separately is the kind of thing that
  // works until one caller forgets.
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_insert_btreevalue(BTreeKey<_First>& key, const Long& value)
  {
    BTreeValue btreevalue;
    btreevalue.value = value;
    btreevalue.next_address = key.values_address;

    btreevalue.pointer = this->_memory->_allocate(this->_hash_key, btreevalue.pack_size());

    if (this->_is_catalogue) {

      // The catalogue is written offline, so its index has to be written offline
      // too. _insert_btreeindex puts the BTreeRecord row down with _dump_control
      // and an offline INSERTED state -- metadata, true the moment it is
      // written, holding no lock and belonging to no transaction. Its index
      // entry went in through _control_insert instead, which takes an EXCLUSIVE
      // lock and waits for a commit. If the request that registered the index
      // never commits, or the server is killed before it does, that lock
      // outlives the process and stays in the store.
      //
      // What it costs is out of all proportion to a metadata row. Every index
      // looks itself up in the catalogue from a *static initialiser*, so the
      // next server to load that object waits out the lock timeout and then
      // throws from inside __static_initialization_and_destruction_0 under
      // dlopen, where no catch can reach it. The process dies, on a read.
      //
      // Nothing was hiding this: the bucket a value used to live in was not
      // where the lookup went looking, so the entry was quietly never found.
      // Chaining the values off the key is what made it visible.
      Control control;
      control.offline_state = RowState::INSERTED;
      control.create_time = Memory::version_time();
      this->_memory->_dump_control(btreevalue.pointer, control);

    } else {

      this->_memory->_transaction_push(btreevalue.pointer);
      this->_memory->_control_insert(&btreevalue);
    }

    this->_memory->_full_hexmap(btreevalue.pointer);
    this->_memory->_data_io.seekp(this->_memory->_pointer_data_address(btreevalue.pointer), std::ios::beg);
    this->_memory->_data_io << btreevalue;

    key.values_address = btreevalue.pointer.address;
    this->_memory->_offline_update(key);

    this->_memory->_hexmap_io.flush();
    this->_memory->_data_io.flush();
  }
    
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_delete_btreevalue(BTreeValue& value)
  {
    this->_memory->_transaction_push(value.pointer);
    this->_memory->_control_delete(&value, Memory::Streams::held());

    this->_memory->_hexmap_io.flush();
    this->_memory->_data_io.flush();
  }
  
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_keys(BTreeNode& node, std::function<bool (int16_t, BTreeKey<_First>&)>&& callback)
  {
    Long tmp_address = node.keys_address;
    int16_t counter = 0;
    while (tmp_address.value() > -1) {

      BTreeKey<_First> key;
      key.pointer = this->_memory->_pointer(this->_hash_key, tmp_address);
      this->_memory->_offline_select(key);

      tmp_address = key.right_address;

      if ( !callback(counter, key) )
	break;

      counter++;
    }
  }

  // The values of one key, newest first.
  //
  // This was a Memory::_cursor over the key's own hash key, which is where the
  // isolation rules came from for free: a value written by a transaction that
  // has not committed, or one that was rolled back, was simply not walked past.
  // A chain has no such scan behind it, so each link is asked the same question
  // directly -- see Memory::_visible.
  //
  // The next address is read before the callback runs, so a callback that
  // deletes the value it was handed does not lose the rest of the chain.
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_values(const BTreeKey<_First>& key, std::function<bool (int64_t, BTreeValue&)>&& callback)
  {
    int64_t counter = -1;
    Long tmp_address = key.values_address;

    while (tmp_address.value() > -1) {

      Pointer pointer = this->_memory->_pointer(this->_hash_key, tmp_address);

      BTreeValue value;
      this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(pointer), std::ios::beg);
      this->_memory->_data_io >> value;

      tmp_address = value.next_address;

      // Asked after the record is read and the chain has been advanced, because
      // under SNAPSHOT the answer is an older version of it: the pointer moves,
      // and the value has to be read again from wherever it moved to.
      // The guard is handed on so that a wait for somebody else's row lock
      // gives the streams back while it waits. Passing null here made every
      // such wait hold the whole store still for the length of it.
      Pointer visible_pointer = pointer;
      if (!this->_memory->_visible(visible_pointer, Memory::Streams::held())) continue;

      if (visible_pointer.address != pointer.address) {
	this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(visible_pointer), std::ios::beg);
	this->_memory->_data_io >> value;
      }

      value.pointer = visible_pointer;

      counter++;
      if ( !callback(counter, value) )
	break;
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_split_node(path_t& ancestors, BTreeNode& node)
  {
    BTreeNode new_node;

    // The upper half of the split is the same kind of node as the half it came
    // from. BTreeNode defaults is_internal to false, and leaving it there turned
    // the right half of a split internal node into a leaf: its children stopped
    // being descended and every key underneath them left the index.
    new_node.is_internal = node.is_internal;

    this->_memory->_offline_insert(this->_hash_key, new_node);

    BTreeKey<_First> left_key;
    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (current_index == this->_min_degree) { // found d + 1 insert in parent

	  left_key.right_address = -1;
	  node.degree = this->_min_degree;
	  
	  this->_memory->_offline_update(node);
	  this->_memory->_offline_update(left_key);

	  current_key.left_node_address = node.pointer.address;
	  current_key.left_address = -1;
	  current_key.right_address = -1;
	  current_key.right_node_address = new_node.pointer.address;

	  this->_memory->_offline_update(current_key);

	  if (ancestors.empty()) { // is root

	    BTreeNode root;
	    root.is_internal = true;
	    this->_memory->_offline_insert(this->_hash_key, root);

	    path_t none;
            this->_map_to_internal(none, root, current_key);

	    this->_root_address = root.pointer.address;

	    // A DEPENDENT level has no catalogue record: its _pointer is the
	    // default Pointer, and _update_btreeindex through it wrote a
	    // control block and record at ADDRESS ZERO, over the first
	    // page's header. The dependent's new root travels back through
	    // the parent key instead -- see _map_callback.
	    if (!this->_is_dependent)
	      this->_update_btreeindex();

	  } else {

	    // The promoted key goes to the immediate parent, and the parent's own
	    // ancestors travel with it. Passing an empty path here made every
	    // overflowing parent believe it was the root: it grew a fresh root of
	    // its own and the real one, along with its other children, was
	    // dropped from the index.
	    BTreeNode* parent_node = ancestors.back();
	    path_t above(ancestors.begin(), ancestors.end() - 1);

	    this->_map_to_internal(above, *parent_node, current_key);
	  }
	
        } else if (current_index > this->_min_degree) { // first key of new node

	  current_key.left_address = -1;
	  new_node.degree = this->_min_degree;
	  new_node.keys_address = current_key.pointer.address;
	  
	  this->_memory->_offline_update(new_node);
	  this->_memory->_offline_update(current_key);

	  return false;
	}

	left_key = current_key;
	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_map_to_internal(path_t& ancestors, BTreeNode& node, BTreeKey<_First>& new_key)
  {
    if (node.keys_address.value() == -1) {
      
      node.degree = 1;
      node.keys_address = new_key.pointer.address;
      this->_memory->_offline_update(node);

      return;
    }
    
    BTreeKey<_First> left_key;	    
    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (new_key.key < current_key.key) {
	  
	  if (current_index == 0) { // first key

	    new_key.right_address = current_key.pointer.address;
	    this->_memory->_offline_update(new_key);
	    
	    node.degree = node.degree.value() + 1;
	    node.keys_address = new_key.pointer.address;
	    this->_memory->_offline_update(node);
	    
	    current_key.left_address = new_key.pointer.address;
	    current_key.left_node_address = new_key.right_node_address;
	    this->_memory->_offline_update(current_key);
	      
	    return false;
	  } else {

	    new_key.left_address = left_key.pointer.address;
	    new_key.right_address = current_key.pointer.address;
	    this->_memory->_offline_update(new_key);
	    
	    node.degree = node.degree.value() + 1;
	    this->_memory->_offline_update(node);
	    
	    left_key.right_address = new_key.pointer.address;
	    this->_memory->_offline_update(left_key);

	    current_key.left_address = new_key.pointer.address;
	    current_key.left_node_address = new_key.right_node_address;
	    this->_memory->_offline_update(current_key);
	      
	    return false;
	  }
	  
	} else {

          if (current_index == node.degree.value() - 1) { // last key

	    new_key.left_address = current_key.pointer.address;
	    this->_memory->_offline_update(new_key);
	    
	    node.degree = node.degree.value() + 1;
	    this->_memory->_offline_update(node);

	    current_key.right_address = new_key.pointer.address;
	    this->_memory->_offline_update(current_key);
		
	    return false;
	  }
	  
	}

	left_key = current_key;
	return true;
      });

    if (node.degree.value() > this->_max_degree) {
      this->_split_node(ancestors, node);
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_map_callback(const _Table& object, const _First& key, const Long& value, BTreeKey<_First>& current_key)
  {

  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_map(path_t& ancestors, BTreeNode& node, const _Table& object, const _First& key, const Long& value)
  {
    if (node.keys_address.value() == -1) {

      BTreeKey<_First> new_key;
      new_key.key = key;
      this->_memory->_offline_insert(this->_hash_key, new_key);

      this->_insert_btreevalue(new_key, value);
	    
      node.degree = 1;
      node.keys_address = new_key.pointer.address;
      this->_memory->_offline_update(node);

      this->_map_callback(object, key, value, new_key);
      return;
    }
    
    BTreeKey<_First> left_key;
    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (key < current_key.key) {
	  
	  if (node.is_internal) {

	    BTreeNode left_node = this->_btreenode(current_key.left_node_address);
	    ancestors.push_back(&node);
	    this->_map(ancestors, left_node, object, key, value);
	    ancestors.pop_back();
	    return false;

	  } else {
            if (current_index == 0) { // first key

	      BTreeKey<_First> new_key;
	      new_key.key = key;
	      new_key.right_address = current_key.pointer.address;
	      this->_memory->_offline_insert(this->_hash_key, new_key);

	      this->_insert_btreevalue(new_key, value);
	    
	      node.degree = node.degree.value() + 1;
	      node.keys_address = new_key.pointer.address;
	      this->_memory->_offline_update(node);
	      
	      current_key.left_address = new_key.pointer.address;
	      this->_memory->_offline_update(current_key);
	      
	      this->_map_callback(object, key, value, new_key);
	      return false;
	    } else {

	      BTreeKey<_First> new_key;
	      new_key.key = key;
	      new_key.left_address = left_key.pointer.address;
	      new_key.right_address = current_key.pointer.address;
	      this->_memory->_offline_insert(this->_hash_key, new_key);

	      this->_insert_btreevalue(new_key, value);
	    
	      node.degree = node.degree.value() + 1;
	      this->_memory->_offline_update(node);
	      
	      left_key.right_address = new_key.pointer.address;
	      this->_memory->_offline_update(left_key);

	      current_key.left_address = new_key.pointer.address;
	      this->_memory->_offline_update(current_key);
	      
	      this->_map_callback(object, key, value, new_key);
	      return false;
	    }
	  }
	  
	} else {

	  if (key == current_key.key) {

	    if (this->_is_unique && !this->_is_multi_level) {
	      this->_cursor_values(current_key, [&] (int64_t, BTreeValue& current_value) -> bool {
		  throw IndexException("unique key '" + this->_name + "'");
		});  
	    }

	    if (!this->_is_multi_level)
	      this->_insert_btreevalue(current_key, value);
	    
	    this->_map_callback(object, key, value, current_key);
	    return false;

	  } else if (node.is_internal) {
	    
	    if (current_index == node.degree.value() - 1) { // last key

	      BTreeNode right_node = this->_btreenode(current_key.right_node_address);
	      ancestors.push_back(&node);
	      this->_map(ancestors, right_node, object, key, value);
	      ancestors.pop_back();
	      return false;

	    }
	    
	  } else {
            if (current_index == node.degree.value() - 1) { // last key

	      BTreeKey<_First> new_key;
	      new_key.key = key;
	      new_key.left_address = current_key.pointer.address;
	      this->_memory->_offline_insert(this->_hash_key, new_key);

	      this->_insert_btreevalue(new_key, value);
	    
	      node.degree = node.degree.value() + 1;
	      this->_memory->_offline_update(node);
	      
	      current_key.right_address = new_key.pointer.address;
	      this->_memory->_offline_update(current_key);
	      	
	      this->_map_callback(object, key, value, new_key);
	      return false;
	    }
	  }
	  
	}

	left_key = current_key;
	return true;
      });

    if (node.degree.value() > this->_max_degree) {
      this->_split_node(ancestors, node);
    }

  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::map(const _Table& object)
  {
    if (this->_memory->_watcher) {
      this->_memory->dba_watch("BTreeIndex::map: (" + this->_name + ", " + (object.*this->_column).to_string().value() + ")");
    }

    // Reached from Memory::online_insert, which is already holding these, and
    // also from BTreeRecord's own catalogue registration, which is not. The
    // guard is a no-op in the first case and the real thing in the second.
    Memory::Streams streams(this->_memory);

    if (this->_root_address == -1) {

      BTreeNode root;
      this->_memory->_offline_insert(this->_hash_key, root);
      this->_root_address = root.pointer.address;

      if (!this->_is_dependent)
	this->_update_btreeindex();
    }

    BTreeNode root_node = this->_btreenode(this->_root_address);
    Long value = object.pointer.address;
    path_t ancestors;
    this->_map(ancestors, root_node, object, object.*this->_column, value);
  }
  
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_unmap_callback(const _Table& object, const _First& key, const Long& value, BTreeKey<_First>& current_key)
  {

  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_unmap(BTreeNode& node, const _Table& object, const _First& key, const Long& value)
  {
    BTreeKey<_First> left_key;
    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {
	if (key < current_key.key) {

	  if (node.is_internal) {

	    BTreeNode left_node = this->_btreenode(current_key.left_node_address);
            this->_unmap(left_node, object, key, value);

	    return false;
	  } 

	} else if (key == current_key.key) {

	  if (this->_is_multi_level) {

	    // The values of a composite index hang off its innermost level, so
	    // this level has none of its own and the walk below would never run
	    // -- which left the dependent index holding a deleted row.
	    this->_unmap_callback(object, key, value, current_key);

	  } else {

	    // One row's entry, not the key's whole chain. A non unique key holds
	    // a value for every row that shares it, and deleting all of them took
	    // every one of those rows out of the index along with the one that
	    // was asked for.
	    this->_cursor_values(current_key, [&] (int64_t, BTreeValue& current_value) -> bool {
		if (!(current_value.value == value)) return true;

		this->_unmap_callback(object, key, value, current_key);
		this->_delete_btreevalue(current_value);
		return false;
	      });
	  }
	  return false;

	} else {

	  if (node.is_internal && current_key.right_address.value() == -1) {

	    BTreeNode right_node = this->_btreenode(current_key.right_node_address);
            this->_unmap(right_node, object, key, value);  
	  }
	  
	}

	left_key = current_key;
	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::unmap(const _Table& object)
  {
    if (this->_memory->_watcher) {
      this->_memory->dba_watch("BTreeIndex::unmap: (" + this->_name + ", " + (object.*this->_column).to_string().value() + ")");
    }

    Memory::Streams streams(this->_memory);

    if (this->_root_address != -1) {

      BTreeNode root_node = this->_btreenode(this->_root_address);
      Long value = object.pointer.address;
      this->_unmap(root_node, object, object.*this->_column, value);
    }
  }
  
  // Gives the whole chain back, for a key that is leaving the tree.
  //
  // The bucket version of this called Memory::_free_key, which drops every page
  // of a hash key -- correct when the key owned a hash key of its own, and
  // catastrophic now that the hash key is the index's. Each value is freed by
  // address instead, and the chain is read one link ahead of the freeing.
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_free_key_values(BTreeKey<_First>& key)
  {
    Long tmp_address = key.values_address;

    while (tmp_address.value() > -1) {

      Pointer pointer = this->_memory->_pointer(this->_hash_key, tmp_address);

      BTreeValue value;
      this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(pointer), std::ios::beg);
      this->_memory->_data_io >> value;

      tmp_address = value.next_address;

      this->_memory->_free(pointer);
    }

    key.values_address = -1;
  }
  
  // A dead value cannot simply be freed: it is a link, and the link before it
  // names it by address. Free its chunks while something still points at them
  // and the next allocation is handed a record another chain believes it owns.
  // So the chain is closed over the dead link first, and the space is asked for
  // afterwards.
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_unlink_dead_values(BTreeKey<_First>& key)
  {
    bool has_previous = false;
    BTreeValue previous;
    Long tmp_address = key.values_address;

    while (tmp_address.value() > -1) {

      Pointer pointer = this->_memory->_pointer(this->_hash_key, tmp_address);

      BTreeValue value;
      this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(pointer), std::ios::beg);
      this->_memory->_data_io >> value;
      value.pointer = pointer;

      tmp_address = value.next_address;

      Control control;
      this->_memory->_load_control(pointer, control);

      // Settled, in the sense _dead_pointers means it: the delete is committed
      // and no live transaction is still holding the record. One that a session
      // is mid-flight on is left exactly where it is.
      if (control.offline_state == RowState::DELETED &&
	  control.online_state == RowState::NONE &&
	  control.online_lock == RowLock::NONE) {

	if (has_previous) {
	  previous.next_address = value.next_address;
	  this->_memory->_offline_update(previous);
	} else {
	  key.values_address = value.next_address;
	  this->_memory->_offline_update(key);
	}

      } else {
	previous = value;
	has_previous = true;
      }
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_unlink_dead()
  {
    Memory::Streams streams(this->_memory);

    if (this->_root_address == -1) return;

    this->_cursor(this->_root_address, [&] (BTreeKey<_First>& current_key) -> bool {
	this->_unlink_dead_values(current_key);
	return true;
      });
  }

  template <typename _Table, typename _First>
  size_t BTreeIndex<_Table, _First>::truncate()
  {
    this->_unlink_dead();
    return this->_memory->truncate(this->_hash_key);
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_combine_nodes(BTreeNode& node, BTreeKey<_First>& key)
  {
    if (key.left_address.value() != -1) {
      BTreeKey<_First> left_key = this->_btreekey(key.left_address);
      left_key.right_address = key.right_address;
      this->_memory->_offline_update(left_key);
    } else {
      node.keys_address = key.right_address;
    }

    if (key.right_address.value() != -1) {
      BTreeKey<_First> right_key = this->_btreekey(key.right_address);
      right_key.left_node_address = key.left_node_address;
      right_key.left_address = key.left_address;
      this->_memory->_offline_update(right_key);
    }
    
    BTreeNode left_node = this->_btreenode(key.left_node_address);
    BTreeNode right_node = this->_btreenode(key.right_node_address);
    
    BTreeKey<_First> left_key;
    this->_cursor_keys(left_node, [&left_key] (int16_t, BTreeKey<_First>& current_key) -> bool {

        left_key = current_key;
	return true;
      });
    
    left_key.right_address = key.pointer.address;
    this->_memory->_offline_update(left_key);
    
    this->_cursor_keys(right_node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	// THE BOUNDARY CHILDREN ARE ADOPTED, NOT ZEROED. When the merged
	// children are internal, the left child's last key and the right
	// child's first key each hang a subtree toward the seam, and the
	// demoted separator is what now sits between them -- zeroing its
	// child links dropped both subtrees from the index. When the
	// children are leaves both links are -1 anyway, so the adoption is
	// the general case and the old zeroing was the leaf-only special.
	key.left_node_address = left_key.right_node_address;
        key.left_address = left_key.pointer.address;
	key.right_address = current_key.pointer.address;
	key.right_node_address = current_key.left_node_address;
	this->_memory->_offline_update(key);

	current_key.left_address = key.pointer.address;
	this->_memory->_offline_update(current_key);

	return false;
      });

    node.degree = node.degree.value() - 1;
    this->_memory->_offline_update(node);

    left_node.degree = left_node.degree.value() + 1 + right_node.degree.value();
    this->_memory->_offline_update(left_node);
    
    this->_memory->_offline_delete(right_node);

    if (left_node.degree.value() > this->_max_degree) {
      // Only the immediate parent is known here, which is as much as this path
      // ever had. It is reached from unmap_key, which nothing calls yet.
      path_t ancestors(1, &node);
      this->_split_node(ancestors, left_node);
    }

    // THE ROOT SHRINKS ONLY WHEN THE EMPTIED PARENT IS THE ROOT. This used
    // to reassign the root for ANY node reaching degree zero -- a transient
    // state a cascading merge passes through -- pointing the whole index at
    // an interior node. And a dependent level has no catalogue record to
    // update; its root travels back through its parent key.
    if (node.degree == Short(Int(0)) && this->_root_address.value() == node.pointer.address) {
      this->_root_address = left_node.pointer.address;
      if (!this->_is_dependent)
	this->_update_btreeindex();
    }
  }
  
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_unmap_key_callback(const _Table& object, const _First& key, BTreeKey<_First>& current_key)
  {

  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_unmap_key(BTreeNode* parent_node, BTreeKey<_First>* parent_key, BTreeNode& node, const _Table& object, const _First& key)
  {
    BTreeKey<_First> left_key;
    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (key < current_key.key) {

	  if (node.is_internal) {

	    BTreeNode left_node = this->_btreenode(current_key.left_node_address);
            this->_unmap_key(&node, &current_key, left_node, object, key);

	    return false;
	  } 

	} else if (key == current_key.key) {

	  if (node.is_internal) {

	    // AN INTERNAL KEY LEAVES BY SUCCESSOR REPLACEMENT. What stood
	    // here lifted the right child's FIRST key in as the replacement
	    // -- the in-order successor only when that child is a LEAF --
	    // after a recursive call that walked the right subtree for a key
	    // that is not in it (a no-op); relinked the right neighbour to
	    // the OLD key's left neighbour instead of the replacement;
	    // assigned nullptr into a Long -- a NULL the store cannot read
	    // back; and ran the underflow combine on the key AFTER
	    // _offline_delete had freed its record, resurrecting chunks the
	    // allocator owned into the tree.
	    //
	    // Now: this key's own values go back first, the leftmost LEAF
	    // key of the right subtree lends its payload -- key, values,
	    // dependents -- to this record while the links stay, the leaf
	    // copy is stripped so its deletion frees nothing that moved, and
	    // the successor is deleted out of the right subtree by the
	    // ordinary recursion, whose own end-of-walk check runs any
	    // underflow combine against the LIVE separator.
	    this->_unmap_key_callback(object, key, current_key);
	    this->_free_key_values(current_key);

	    BTreeKey<_First> successor = this->_leftmost_key(current_key.right_node_address);

	    current_key.key = successor.key;
	    current_key.values_address = successor.values_address;
	    current_key.dependents_address = successor.dependents_address;
	    this->_memory->_offline_update(current_key);

	    successor.values_address = -1;
	    successor.dependents_address = -1;
	    this->_memory->_offline_update(successor);

	    BTreeNode right_child_node = this->_btreenode(current_key.right_node_address);
	    this->_unmap_key(&node, &current_key, right_child_node, object, current_key.key);

	    return false;

	  } else {
	    if (left_key.pointer.hash_key == nullptr) { // first

	      node.degree = node.degree.value() - 1;
	      node.keys_address = current_key.right_address;
	      this->_memory->_offline_update(node);

	      if (current_key.right_address.value() != -1) {
		BTreeKey<_First> right_key = this->_btreekey(current_key.right_address);
		right_key.left_address = -1;
		this->_memory->_offline_update(right_key);
	      }

	      this->_unmap_key_callback(object, key, current_key);
	      this->_free_key_values(current_key);
	      this->_memory->_offline_delete(current_key);

	      return false;
	      
	    } else if (current_index == node.degree.value() - 1) { // last

	      node.degree = node.degree.value() - 1;
	      this->_memory->_offline_update(node);

	      left_key.right_address = -1;
	      this->_memory->_offline_update(left_key);	      

	      this->_unmap_key_callback(object, key, current_key);
	      this->_free_key_values(current_key);
	      this->_memory->_offline_delete(current_key);

	      return false;
	      
	    } else { // remined

	      node.degree = node.degree.value() - 1;
	      this->_memory->_offline_update(node);

	      left_key.right_address = current_key.right_address;
	      this->_memory->_offline_update(left_key);

	      BTreeKey<_First> right_key = this->_btreekey(current_key.right_address);
	      right_key.left_address = current_key.left_address;
	      this->_memory->_offline_update(right_key);

	      this->_unmap_key_callback(object, key, current_key);
	      this->_free_key_values(current_key);
	      this->_memory->_offline_delete(current_key);

	      return false;
	    }
	  }	  
	} else {
          if (node.is_internal && current_key.right_address.value() == -1) {

	    BTreeNode right_node = this->_btreenode(current_key.right_node_address);
            this->_unmap_key(&node, &current_key, right_node, object, key);  
	  }
	  
	  return false;
	}

	left_key = current_key;
	return true;
      });

    if (node.degree.value() < this->_min_degree && parent_node != nullptr) {
      this->_combine_nodes(*parent_node, *parent_key);
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::unmap_key(const _Table& object)
  {
    if (this->_memory->_watcher) {
      this->_memory->dba_watch("BTreeIndex::unmap_key: (" + this->_name + ", " + (object.*this->_column).to_string().value() + ")");
    }

    Memory::Streams streams(this->_memory);

    if (this->_root_address != -1) {

      BTreeNode root_node = this->_btreenode(this->_root_address);
      this->_unmap_key(nullptr, nullptr, root_node, object, object.*this->_column);
    }
  }


  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_output(const BTreeKey<_First>& key, const std::function<bool (_Table&)>& callback)
  {
    // True unless the caller's callback asks to stop. An unmapped row leaves its
    // key behind with an empty bucket, and an empty bucket is not a reason to
    // end the walk -- starting this at false ended the whole cursor at the first
    // key whose rows had been deleted.
    bool do_continue = true;
    this->_cursor_values(key, [&] (int64_t, BTreeValue& current_value) -> bool {

	_Table object(this->_memory->_pointer(_Table::hash_key, current_value.value));

	this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(object.pointer), std::ios::beg);
	this->_memory->_data_io >> object;

	// THE STREAMS GO BACK BEFORE THE CALLER'S CALLBACK, exactly as
	// Memory::_cursor does for a table scan. The callback is procedure code
	// and the ordinary thing for it to do is update the row it has just been
	// handed -- which takes these same two mutexes. Holding them across it
	// would deadlock every UPDATE ... WHERE that goes through an index.
	Memory::Streams* streams = Memory::Streams::held();
	if (streams) streams->unlock();

	do_continue = callback(object);

	if (streams) streams->lock();
	return do_continue;
      });

    return do_continue;
  }

  // How the children of an internal node hang off its keys, because every
  // cursor below depends on it:
  //
  //     c0   k0   c1   k1   c2   k2   c3
  //
  // c0 is k0.left_node_address, and c(i+1) is k(i).right_node_address. The keys
  // are a linked list, and _map_to_internal keeps k(i).right_node_address and
  // k(i+1).left_node_address pointing at the same node -- consecutive keys share
  // a child. So an in-order walk descends the left child of the *first* key
  // only, and then alternates key, right child. Descending the left child of
  // every key visits c1..c(m-1) twice.
  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor(const Long& node_address, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;

    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (node.is_internal && current_index == 0) {
	  do_continue = this->_cursor(current_key.left_node_address, callback);
	  if (!do_continue) return false;
	}

	do_continue = callback(current_key);
	if (!do_continue) return false;

	if (node.is_internal) {
	  do_continue = this->_cursor(current_key.right_node_address, callback);
	  if (!do_continue) return false;
	}

	return true;
      });

    return do_continue;
  }


  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor(const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor(this->_root_address, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});    
    }
  }

  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (key < current_key.key) {

	  // Everything from here on is larger, so the key is either in the child
	  // between the previous key and this one, or not in the tree at all.
	  if (node.is_internal)
	    do_continue = this->_cursor_equal(current_key.left_node_address, key, callback);

	  return false;

	} else if (key == current_key.key) {

	  do_continue = callback(current_key);
	  return false;

	} else {

	  // Larger than this key: keep scanning the node. Only once the last key
	  // has been passed does the rightmost child become the one to descend --
	  // descending here would skip every key after this one.
	  if (node.is_internal && current_key.right_address.value() == -1) {
	    do_continue = this->_cursor_equal(current_key.right_node_address, key, callback);
	    return false;
	  }

	}

	return true;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});  
    }
  }

  // A full walk with one key held back.
  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_not_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;

    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (node.is_internal && current_index == 0) {
	  do_continue = this->_cursor_not_equal(current_key.left_node_address, key, callback);
	  if (!do_continue) return false;
	}

	if (!(key == current_key.key)) {
	  do_continue = callback(current_key);
	  if (!do_continue) return false;
	}

	if (node.is_internal) {
	  do_continue = this->_cursor_not_equal(current_key.right_node_address, key, callback);
	  if (!do_continue) return false;
	}

	return true;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_not_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor_not_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  // Walks in order from the smallest key and stops at the bound. The leftmost
  // child is descended before the first key is tested, because it holds keys
  // below it that may still qualify.
  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_less_than(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;

    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (node.is_internal && current_index == 0) {
	  do_continue = this->_cursor_less_than(current_key.left_node_address, key, callback);
	  if (!do_continue) return false;
	}

	// This key and every key after it is at or above the bound. The child
	// that straddles the bound has already been walked.
	if (!(current_key.key < key)) return false;

	do_continue = callback(current_key);
	if (!do_continue) return false;

	if (node.is_internal) {
	  do_continue = this->_cursor_less_than(current_key.right_node_address, key, callback);
	  if (!do_continue) return false;
	}

	return true;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_less_than(const _First key, const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor_less_than(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_less_than_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;

    this->_cursor_keys(node, [&] (int16_t current_index, BTreeKey<_First>& current_key) -> bool {

	if (node.is_internal && current_index == 0) {
	  do_continue = this->_cursor_less_than_equal(current_key.left_node_address, key, callback);
	  if (!do_continue) return false;
	}

	if (key < current_key.key) return false;

	do_continue = callback(current_key);
	if (!do_continue) return false;

	// Keys are distinct in the tree -- a non unique index buckets its
	// duplicates under one key -- so once the bound itself has been emitted
	// there is nothing above it left to match.
	if (key == current_key.key) return false;

	if (node.is_internal) {
	  do_continue = this->_cursor_less_than_equal(current_key.right_node_address, key, callback);
	  if (!do_continue) return false;
	}

	return true;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_less_than_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor_less_than_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  // Skips forward to the bound and then walks in order to the end. Only the
  // first qualifying key needs its left child descended: the child straddles the
  // bound, and every later key's left child is the previous key's right child,
  // already walked.
  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_greater_than(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;
    bool started = false;

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (!started) {

	  if (!(key < current_key.key)) {
	    // Still at or below the bound. The rightmost child is the only one
	    // left to try once the keys run out.
	    if (node.is_internal && current_key.right_address.value() == -1) {
	      do_continue = this->_cursor_greater_than(current_key.right_node_address, key, callback);
	      return false;
	    }
	    return true;
	  }

	  started = true;

	  if (node.is_internal) {
	    do_continue = this->_cursor_greater_than(current_key.left_node_address, key, callback);
	    if (!do_continue) return false;
	  }
	}

	do_continue = callback(current_key);
	if (!do_continue) return false;

	if (node.is_internal) {
	  do_continue = this->_cursor_greater_than(current_key.right_node_address, key, callback);
	  if (!do_continue) return false;
	}

	return true;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_greater_than(const _First key, const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor_greater_than(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_greater_than_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);
    bool do_continue = true;
    bool started = false;

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (!started) {

	  if (current_key.key < key) {
	    if (node.is_internal && current_key.right_address.value() == -1) {
	      do_continue = this->_cursor_greater_than_equal(current_key.right_node_address, key, callback);
	      return false;
	    }
	    return true;
	  }

	  started = true;

	  if (node.is_internal) {
	    do_continue = this->_cursor_greater_than_equal(current_key.left_node_address, key, callback);
	    if (!do_continue) return false;
	  }
	}

	do_continue = callback(current_key);
	if (!do_continue) return false;

	if (node.is_internal) {
	  do_continue = this->_cursor_greater_than_equal(current_key.right_node_address, key, callback);
	  if (!do_continue) return false;
	}

	return true;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_greater_than_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    // EVERY WALK OF THIS TREE IS UNDER THE STREAMS. It reads nodes, keys and
    // values out of the same _hexmap_io and _data_io every table does, and
    // procedure code arrives here holding nothing -- so without this, an
    // indexed WHERE ran straight through the store while another connection
    // was writing it. _cursor_output hands them back around the callback.
    Memory::Streams streams(this->_memory);

    // And it is a statement, like a table scan is: one view of the store for the
    // whole walk, so a row rewritten while it is going on is met once rather
    // than twice or not at all. Nested inside a table scan this does nothing --
    // the scan's view is already the one that counts.
    Memory::Statement statement(this->_memory);

    if (this->_root_address > -1) {
      this->_cursor_greater_than_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First, typename ..._Rest>
  class BTreeIndex<_Table, _First, _Rest...> : public BTreeIndex<_Table, _First>
  {
  private:
    std::tuple<_Rest _Table::*...> _columns;
  
  public:
    using BTreeIndex<_Table, _First>::BTreeIndex;
    BTreeIndex(Memory* memory, std::string name, bool is_unique, 
	       _First _Table::* column, _Rest _Table::*... columns)
      : BTreeIndex(memory, name, is_unique, column)
    {
      this->_is_multi_level = true;
      this->_columns = std::make_tuple(std::move(columns)...);
    }

    BTreeIndex(Memory* memory, std::string name, bool is_unique, std::string hash_name, Long pointer_address, 
	       std::tuple<_First _Table::*, _Rest _Table::*...> columns) 
      : BTreeIndex(memory, name, is_unique, hash_name, pointer_address, std::get<0>(columns))
    {
      this->_is_multi_level = true;
      this->_columns = Utility::tuple_cdr<_First, _Rest...>(columns);
    }

    virtual void _map_callback(const _Table& object, const _First& key, const Long& value, BTreeKey<_First>& current_key) override
    {
      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
						   this->_hash_name, current_key.dependents_address, this->_columns);

      dependent_index.map(object);

      // WHENEVER IT CHANGED, not only on creation. A dependent level's root
      // moves when it splits, and writing it back only from -1 left the
      // parent pointing at the old root -- the lower half -- so every key
      // promoted above it silently left the index the moment one
      // first-column key held more than 2d second-column keys.
      if (dependent_index.root_address().value() != current_key.dependents_address.value()) {
	current_key.dependents_address = dependent_index.root_address();
	this->_memory->_offline_update(current_key);
      }
    }

    virtual void _unmap_callback(const _Table& object, const _First& key, const Long& value, BTreeKey<_First>& current_key) override
    {
      if (current_key.dependents_address.value() > -1) {
	BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
						     this->_hash_name, current_key.dependents_address, this->_columns);
	dependent_index.unmap(object);
      }
    }

    // Down through the dependents. This level's keys hold no values of their
    // own -- _map only inserts them where _is_multi_level is false -- so the
    // base version would find every chain already empty and unlink nothing.
    virtual void _unlink_dead() override
    {
      if (this->_root_address == -1) return;

      this->_cursor(this->_root_address, [&] (BTreeKey<_First>& current_key) -> bool {

	  if (current_key.dependents_address.value() > -1) {
	    BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique,
							 this->_hash_name, current_key.dependents_address, this->_columns);
	    dependent_index._unlink_dead();
	  }

	  return true;
	});
    }

    // Two hash keys to sweep, not one. Every dependent level of one index
    // shares a hash key -- the dependent constructor takes it from the hash
    // name, which is passed down unchanged -- and that is not the hash key of
    // this level, which is the checksum of the index name.
    size_t truncate()
    {
      this->_unlink_dead();

      size_t freed = this->_memory->truncate(this->_hash_key);

      hashkey_t dependent_hash_key;
      std::memcpy(dependent_hash_key, this->_hash_name.c_str(), Memory::HASHKEY_SIZE);
      freed += this->_memory->truncate(dependent_hash_key);

      return freed;
    }

    virtual void _unmap_key_callback(const _Table& object, const _First& key, BTreeKey<_First>& current_key) override
    {
      if (current_key.dependents_address.value() > -1) {
	BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
						     this->_hash_name, current_key.dependents_address, this->_columns);
	dependent_index.unmap_key(object);
      }
    }

    void cursor(std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor(this->_root_address, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

    void cursor_equal(const _First first, std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor_equal(this->_root_address, first, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

    void cursor_not_equal(const _First first, std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor_not_equal(this->_root_address, first, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

    void cursor_less_than(const _First first, std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor_less_than(this->_root_address, first, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

    void cursor_less_than_equal(const _First first, std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor_less_than_equal(this->_root_address, first, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

    void cursor_greater_than(const _First first, std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor_greater_than(this->_root_address, first, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

    void cursor_greater_than_equal(const _First first, std::function<bool (BTreeIndex<_Table, _Rest...>&)> callback)
    {
      // Under the streams and one statement, for the reasons given on the
      // single-column cursors.
      Memory::Streams streams(this->_memory);
      Memory::Statement statement(this->_memory);

      if (this->_root_address > -1) {
	this->_cursor_greater_than_equal(this->_root_address, first, [&] (BTreeKey<_First>& current_key) -> bool {
	  
	    if (current_key.dependents_address.value() > -1) {
	      BTreeIndex<_Table, _Rest...> dependent_index(this->_memory, this->_name, this->_is_unique, 
							   this->_hash_name, current_key.dependents_address, this->_columns);
	      return callback(dependent_index);
	    }

	    return true;
	  });
      }
    }

  };

}

#endif // __BTREEINDEX_HPP__
