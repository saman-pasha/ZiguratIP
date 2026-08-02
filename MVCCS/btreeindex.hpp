
#ifndef __BTREEINDEX_HPP__
#define __BTREEINDEX_HPP__

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

    void _insert_btreeindex();
    void _select_btreeindex();
    void _update_btreeindex();
    
    void _insert_btreevalue(hashkey_ptr, BTreeValue&);
    void _insert_btreevalue(BTreeKey<_First>&, const Long&);
    void _delete_btreevalue(BTreeValue&);

    BTreeNode _btreenode(const Long&);
    BTreeKey<_First> _btreekey(const Long&);

    void _bucket_key(const BTreeKey<_First>&, hashkey_t);
    
    void _cursor_keys(BTreeNode&, std::function<bool (int16_t, BTreeKey<_First>&)>&&);    
    void _cursor_values(hashkey_ptr, std::function<bool (int64_t, BTreeValue&)>&&);
    void _cursor_values(const BTreeKey<_First>&, std::function<bool (int64_t, BTreeValue&)>&&);

    void _split_node(BTreeNode*, BTreeNode&);
    void _map_to_internal(BTreeNode*, BTreeNode&, BTreeKey<_First>&);
    virtual void _map_callback(const _Table&, const _First&, const Long&, BTreeKey<_First>&);
    void _map(BTreeNode*, BTreeNode&, const _Table&, const _First&, const Long&);

    virtual void _unmap_callback(const _Table&, const _First&, const Long&, BTreeKey<_First>&);
    void _unmap(BTreeNode&, const _Table&, const _First&, const Long&);

    void _free_key_bucket(BTreeKey<_First>&);
    void _combine_nodes(BTreeNode&, BTreeKey<_First>&);
    virtual void _unmap_key_callback(const _Table&, const _First&, BTreeKey<_First>&);
    void _unmap_key(BTreeNode*, BTreeKey<_First>*, BTreeNode&, const _Table&, const _First&);
  
    bool _cursor_output(const BTreeKey<_First>&, const std::function<bool (_Table&)>&);
    
    void _cursor(const Long&, const std::function<bool (BTreeKey<_First>&)>&);
    void _cursor_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    void _cursor_not_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    void _cursor_less_than(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    void _cursor_less_than_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    void _cursor_greater_than(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);
    void _cursor_greater_than_equal(const Long&, const _First&, const std::function<bool (BTreeKey<_First>&)>&);

  public:
    BTreeIndex(Memory*, std::string, bool, _First _Table::*);
    BTreeIndex(Memory*, std::string, bool, std::string, Long, std::tuple<_First _Table::*>);

    void set_memory(Memory*);
    Long root_address();

    void map(const _Table&);
    void unmap(const _Table&);
    void unmap_key(const _Table&);

    void cursor(std::function<bool (_Table&)>);
    void cursor_equal(const _First, std::function<bool (_Table&)>);
    void cursor_not_equal(const _First, std::function<bool (_Table&)>);
    void cursor_less_than(const _First, std::function<bool (_Table&)>);
    void cursor_less_than_equal(const _First, std::function<bool (_Table&)>);
    void cursor_greater_than(const _First, std::function<bool (_Table&)>);
    void cursor_greater_than_equal(const _First, std::function<bool (_Table&)>);
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
  void BTreeIndex<_Table, _First>::_bucket_key(const BTreeKey<_First>& key, hashkey_t hash_key)
  {
    std::string composite_key = this->_name + std::to_string(key.pointer.address);
    SHA::checksum(SHA::SHA1, (const uint8_t*)composite_key.c_str(), composite_key.size(), hash_key);
  } 

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_insert_btreeindex()
  {
    String hash_name_(this->_hash_name);
    Bool is_unique_(this->_is_unique);
    Short branching_factor_(this->_branching_factor);
    Long root_address_(this->_root_address);

    int64_t size = binarystream::pack_size(hash_name_, is_unique_, branching_factor_, root_address_);
    this->_pointer = this->_memory->_allocate(Memory::INDICES_HASHKEY, size);
    
    Control control;
    control.offline_state = RowState::INSERTED;
    control.create_time = std::time(0);
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
    bool found = false;
    if (this->_name == "IDX_ZIGURAT_BTREERECORD_HASH_NAME") {

      std::unique_lock<std::mutex> hexmap_lock(this->_memory->_hexmap_access);
      std::unique_lock<std::mutex> data_lock(this->_memory->_data_access);    

      this->_memory->_cursor(Memory::INDICES_HASHKEY, &hexmap_lock, &data_lock, [&] (const Pointer& index_pointer) -> bool {

	  String hash_name_;
	  Bool is_unique_;
	  Short branching_factor_;
	  Long root_address_;

	  this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(index_pointer), std::ios::beg);
	  this->_memory->_data_io.unpack(hash_name_, is_unique_, branching_factor_, root_address_);

	  if (this->_hash_name == hash_name_.value()) {
	    this->_root_address = root_address_;	
	    this->_pointer = index_pointer;
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
    String hash_name_(this->_hash_name);
    Bool is_unique_(this->_is_unique);
    Short branching_factor_(this->_branching_factor);
    Long root_address_(this->_root_address);

    Control control;
    this->_memory->_load_control(this->_pointer, control);
    control.modify_time = std::time(0);
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
  void BTreeIndex<_Table, _First>::_insert_btreevalue(hashkey_ptr hash_key, BTreeValue& value)
  {
    value.pointer = this->_memory->_allocate(hash_key, value.pack_size());

    this->_memory->_transaction_push(value.pointer);
    this->_memory->_control_insert(&value);
    
    this->_memory->_full_hexmap(value.pointer);
    this->_memory->_data_io.seekp(this->_memory->_pointer_data_address(value.pointer), std::ios::beg);
    this->_memory->_data_io << value;

    this->_memory->_hexmap_io.flush();
    this->_memory->_data_io.flush();
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_insert_btreevalue(BTreeKey<_First>& key, const Long& value)
  {
    BTreeValue btreevalue;
    btreevalue.value = value;

    hashkey_t hash_key;
    this->_bucket_key(key, hash_key);
    
    this->_insert_btreevalue(hash_key, btreevalue);
  }
    
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_delete_btreevalue(BTreeValue& value)
  {
    this->_memory->_transaction_push(value.pointer);
    this->_memory->_control_delete(&value, nullptr, nullptr);

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

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_values(hashkey_ptr hash_key, std::function<bool (int64_t, BTreeValue&)>&& callback)
  {
    int16_t counter = -1;
    this->_memory->_cursor(hash_key, nullptr, nullptr, [&] (const Pointer& pointer) -> bool {

        counter++;
	BTreeValue value;

	this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(pointer), std::ios::beg);
	this->_memory->_data_io >> value;

        value.pointer = pointer;
	return callback(counter, value);

      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_values(const BTreeKey<_First>& key, std::function<bool (int64_t, BTreeValue&)>&& callback)
  {
    hashkey_t hash_key;
    this->_bucket_key(key, hash_key);

    this->_cursor_values(hash_key, std::forward<std::function<bool (int64_t, BTreeValue&)>&&>(callback));
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_split_node(BTreeNode* parent_node, BTreeNode& node)
  {
    BTreeNode new_node;
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

	  if (parent_node == nullptr) { // is root

	    BTreeNode root;
	    root.is_internal = true;
	    this->_memory->_offline_insert(this->_hash_key, root);
            this->_map_to_internal(nullptr, root, current_key);

	    this->_root_address = root.pointer.address;
            this->_update_btreeindex();

	  } else {
	    this->_map_to_internal(nullptr, *parent_node, current_key);
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
  void BTreeIndex<_Table, _First>::_map_to_internal(BTreeNode* parent_node, BTreeNode& node, BTreeKey<_First>& new_key)
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
      this->_split_node(parent_node, node);
    }
  }
  
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_map_callback(const _Table& object, const _First& key, const Long& value, BTreeKey<_First>& current_key)
  {

  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_map(BTreeNode* parent_node, BTreeNode& node, const _Table& object, const _First& key, const Long& value)
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
	    this->_map(&node, left_node, object, key, value);
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
	      this->_map(&node, right_node, object, key, value);
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
      this->_split_node(parent_node, node);
    }
    
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::map(const _Table& object)
  {
    if (this->_memory->_watcher) {
      this->_memory->dba_watch("BTreeIndex::map: (" + this->_name + ", " + (object.*this->_column).to_string().value() + ")");
    }    

    if (this->_root_address == -1) {

      BTreeNode root;
      this->_memory->_offline_insert(this->_hash_key, root);
      this->_root_address = root.pointer.address;

      if (!this->_is_dependent)
	this->_update_btreeindex();
    }

    BTreeNode root_node = this->_btreenode(this->_root_address);
    Long value = object.pointer.address;
    this->_map(nullptr, root_node, object, object.*this->_column, value);
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

	  this->_cursor_values(current_key, [&] (int64_t, BTreeValue& current_value) -> bool {
	      this->_unmap_callback(object, key, value, current_key);
	      if (!this->_is_multi_level)
		this->_delete_btreevalue(current_value);
	      return true;
	    });
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

    if (this->_root_address != -1) {

      BTreeNode root_node = this->_btreenode(this->_root_address);
      Long value = object.pointer.address;
      this->_unmap(root_node, object, object.*this->_column, value);
    }
  }
  
  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_free_key_bucket(BTreeKey<_First>& key)
  {
    hashkey_t hash_key;
    this->_bucket_key(key, hash_key);
    
    this->_memory->_free_key(hash_key);	
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

	key.left_node_address = -1;
        key.left_address = left_key.pointer.address;
	key.right_address = current_key.pointer.address;
	key.right_node_address = -1;
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
      this->_split_node(&node, left_node);
    }

    if (node.degree == Short(Int(0))) {
      this->_root_address = left_node.pointer.address;
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

	    BTreeNode right_child_node = this->_btreenode(current_key.right_node_address);
	    this->_unmap_key(&node, &current_key, right_child_node, object, key);
	    
	    BTreeKey<_First> first_child_key = this->_btreekey(right_child_node.keys_address);

	    right_child_node.keys_address = first_child_key.right_address;
	    right_child_node.degree = right_child_node.degree.value() - 1;

	    if (first_child_key.right_address.value() != -1) {
	      BTreeKey<_First> second_child_key = this->_btreekey(first_child_key.right_address);
	      second_child_key.left_address = nullptr;
	      this->_memory->_offline_update(second_child_key);	      
	    }

	    first_child_key.left_node_address = current_key.left_node_address;
	    first_child_key.left_address = current_key.left_address;
	    first_child_key.right_address = current_key.right_address;
	    first_child_key.right_node_address = current_key.right_node_address;
	    this->_memory->_offline_update(first_child_key);
	    
	    if (current_index == 0) {
	      node.keys_address = first_child_key.pointer.address;
	      this->_memory->_offline_update(node);
	    } else {
	      left_key.right_address = first_child_key.pointer.address;
	      this->_memory->_offline_update(left_key);	    
	    }
	    
	    BTreeKey<_First> right_key = this->_btreekey(current_key.right_address);
	    right_key.left_address = current_key.left_address;
	    this->_memory->_offline_update(right_key);

	    this->_unmap_key_callback(object, key, current_key);
	    this->_free_key_bucket(current_key);
	    this->_memory->_offline_delete(current_key);
	      
	    if (right_child_node.degree.value() < this->_min_degree) {
	      this->_combine_nodes(node, current_key);
	    }
	    
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
	      this->_free_key_bucket(current_key);
	      this->_memory->_offline_delete(current_key);

	      return false;
	      
	    } else if (current_index == node.degree.value() - 1) { // last

	      node.degree = node.degree.value() - 1;
	      this->_memory->_offline_update(node);

	      left_key.right_address = -1;
	      this->_memory->_offline_update(left_key);	      

	      this->_unmap_key_callback(object, key, current_key);
	      this->_free_key_bucket(current_key);
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
	      this->_free_key_bucket(current_key);
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

    if (this->_root_address != -1) {

      BTreeNode root_node = this->_btreenode(this->_root_address);
      this->_unmap_key(nullptr, nullptr, root_node, object, object.*this->_column);
    }
  }


  template <typename _Table, typename _First>
  bool BTreeIndex<_Table, _First>::_cursor_output(const BTreeKey<_First>& key, const std::function<bool (_Table&)>& callback)
  {
    bool do_continue = false;
    this->_cursor_values(key, [&] (int64_t, BTreeValue& current_value) -> bool {

	_Table object(this->_memory->_pointer(_Table::hash_key, current_value.value));

	this->_memory->_data_io.seekg(this->_memory->_pointer_data_address(object.pointer), std::ios::beg);
	this->_memory->_data_io >> object;

	do_continue = callback(object);
	return do_continue;
      });

    return do_continue;
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor(const Long& node_address, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (node.is_internal) {
	  this->_cursor(current_key.left_node_address, callback);
	}

	if (callback(current_key)) {
	  if (node.is_internal) {
	    this->_cursor(current_key.right_node_address, callback);
	  }
	  return true;
	}

	return false;
      });
  }


  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor(const std::function<bool (_Table&)> callback)
  {
    if (this->_root_address > -1) {
      this->_cursor(this->_root_address, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});    
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (key < current_key.key) {
	  
	  if (node.is_internal) {
	    this->_cursor_equal(current_key.left_node_address, key, callback);
	    return false;
	  }
	  
	} else if (key == current_key.key) {
	  
	  callback(current_key);
	  return false;
	    
	} else {
	  
	  if (node.is_internal) {
	    this->_cursor_equal(current_key.right_node_address, key, callback);
	    return false;
	  }
	  
	}

	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    if (this->_root_address > -1) {
      this->_cursor_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});  
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_not_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (key < current_key.key) {
	  
	  if (node.is_internal) {
	    this->_cursor_not_equal(current_key.left_node_address, key, callback);
	  }
	  callback(current_key);

	} else if (key == current_key.key) {
	  	    
	} else {
	  
	  callback(current_key);
	  if (node.is_internal) {
	    this->_cursor_not_equal(current_key.right_node_address, key, callback);
	  }
	  
	}

	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_not_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    if (this->_root_address > -1) {
      this->_cursor_not_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_less_than(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (current_key.key < key) {
	  
	  if (node.is_internal) {
	    this->_cursor_less_than(current_key.left_node_address, key, callback);
	  }
	  callback(current_key);

	} else {
	  return false;
	}

	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_less_than(const _First key, const std::function<bool (_Table&)> callback)
  {
    if (this->_root_address > -1) {
      this->_cursor_less_than(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_less_than_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (current_key.key < key) {
	  
	  if (node.is_internal) {
	    this->_cursor_less_than_equal(current_key.left_node_address, key, callback);
	  }
	  callback(current_key);

	} else if (key == current_key.key) {
	  
	  callback(current_key);
	  return false;

	} else {
	  return false;
	}

	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_less_than_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
    if (this->_root_address > -1) {
      this->_cursor_less_than_equal(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_greater_than(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (current_key.key < key) {
	  
	} else if (key == current_key.key) {

	  if (node.is_internal) {
	    this->_cursor_greater_than_equal(current_key.right_node_address, key, callback);
	  }

	} else {
	  
	  callback(current_key);
	  if (node.is_internal) {
	    this->_cursor_greater_than_equal(current_key.right_node_address, key, callback);
	  }

	}

	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_greater_than(const _First key, const std::function<bool (_Table&)> callback)
  {
    if (this->_root_address > -1) {
      this->_cursor_greater_than(this->_root_address, key, [&] (BTreeKey<_First>& current_key) -> bool {
	  return this->_cursor_output(current_key, callback);
	});
    }
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::_cursor_greater_than_equal(const Long& node_address, const _First& key, const std::function<bool (BTreeKey<_First>&)>& callback)
  {
    BTreeNode node = this->_btreenode(node_address);

    this->_cursor_keys(node, [&] (int16_t, BTreeKey<_First>& current_key) -> bool {

	if (current_key.key < key) {
	  
	} else if (key == current_key.key) {
	  
	  callback(current_key);
	  if (node.is_internal) {
	    this->_cursor_greater_than_equal(current_key.right_node_address, key, callback);
	  }

	} else {

	  if (node.is_internal) {
	    this->_cursor_greater_than_equal(current_key.right_node_address, key, callback);
	  }
	  callback(current_key);

	}

	return true;
      });
  }

  template <typename _Table, typename _First>
  void BTreeIndex<_Table, _First>::cursor_greater_than_equal(const _First key, const std::function<bool (_Table&)> callback)
  {
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

      if (current_key.dependents_address.value() == -1) {
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
