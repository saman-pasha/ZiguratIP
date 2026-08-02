#include "configuration.h"
#include "configurationexception.h"
#include <sstream>
#include <fstream>
#include "utility.h"


namespace Zigurat
{

  Configuration::Configuration()
    : _root("", "", "")
  {

  }
	
  Configuration::Configuration(std::istream& file)
    : _root("", "", "")
  {
    this->load(file);
  }

  Configuration::Configuration(const std::string file_path)
    : _root("", "", "")
  {
    this->load(file_path);
  }

  Option Configuration::root() const
  {
    return this->_root;
  }

  void Configuration::_get_part(std::stringstream& ss, std::string& res, char delim) const
  {
    res.clear();
    int counter = 0;
    char ch;
    do {
      ss.get(ch);
      if (ss.eof()) {
	break;
      } else if (ch == '[') {
	counter++;
	if (counter > 1)
	  res.push_back(ch);
      } else if (ch == ']' && counter == 1) {
	counter--;
	if (counter > 0)
	  res.push_back(ch);
      } else if (counter == 0 && ch == delim) {
	return;
      } else {
	res.push_back(ch);
      }
    } while (!ss.eof());
  }
  
  bool Configuration::_childs(const Option& root, const std::string& path, std::list<const Option*>& result) const
  {
    if (path.length() == 0) {
      result.push_back(&root);
    } else {
      std::stringstream sspath(path);
      std::string sub_path;
      std::string part;
      std::string attr;
      std::string key;
      std::string value;
      this->_get_part(sspath, part, '/');
      std::getline(sspath, sub_path);
      std::stringstream sskv(part);
      std::getline(sskv, key, ':');
      this->_attr_key(attr, key);
      std::getline(sskv, value);
      bool attr_found = false;
      bool key_found = false;
      bool value_found = false;
      if (attr.length() == 0) {
	attr_found = true;
      } else {
	if (attr == root.attr)
	  attr_found = true;
      }
      if (key.length() == 0) {
	key_found = true;
      } else {
	if (key == root.key)
	  key_found = true;
      }
      if (value.length() == 0) {
	value_found = true;
      } else {
	if (value == root.value)
	  value_found = true;
      }
      if (attr_found && key_found && value_found) {
	for (const Option& chopt : root.options) {
	  this->_childs(chopt, sub_path, result);
	}
      }
    }
    if (result.size() > 0)
      return true;
    return false;
  }

  bool Configuration::childs(const std::string path, std::list<const Option*>& result) const
  {
    return this->_childs(this->_root, path, result);
  }

  bool Configuration::childs(const Option& root, const std::string path, std::list<const Option*>& result) const
  {
    return this->_childs(root, path, result);
  }
  
  bool Configuration::childs(const std::string path, std::list<std::string>& path_values) const
  {
    std::list<const Option*> values;
    if (this->childs(path, values)) {
      for (const Option* chopt : values)
	path_values.push_back(chopt->key);
      return true;
    }
    return false;
  }

  bool Configuration::childs(const Option& root, const std::string path, std::list<std::string>& path_values) const
  {
    std::list<const Option*> values;
    if (this->_childs(root, path, values)) {
      for (const Option* chopt : values)
	path_values.push_back(chopt->key);
      return true;
    }
    return false;
  }

  bool Configuration::_list(const Option& root, const std::string& path, std::list<const Option*>& result) const
  {
    std::stringstream sspath(path);
    std::string sub_path;
    std::string part;
    std::string attr;
    std::string key;
    std::string value;
    this->_get_part(sspath, part, '/');
    std::getline(sspath, sub_path);
    std::stringstream sskv(part);
    std::getline(sskv, key, ':');
    this->_attr_key(attr, key);
    std::getline(sskv, value);
    bool attr_found = false;
    bool key_found = false;
    bool value_found = false;
    if (attr.length() == 0) {
      attr_found = true;
    } else {
      if (attr == root.attr)
	attr_found = true;
    }
    if (key.length() == 0) {
      key_found = true;
    } else {
      if (key == root.key)
	key_found = true;
    }
    if (value.length() == 0) {
      value_found = true;
    } else {
      if (value == root.value)
	value_found = true;
    }
    if (attr_found && key_found && value_found) {
      if (sub_path.length() == 0) {
	result.push_back(&root);
      } else {
	for (const Option& chopt : root.options) {
	  this->_list(chopt, sub_path, result);
	}
      }
    }
    if (result.size() > 0)
      return true;
    return false;
  }

  bool Configuration::list(const std::string path, std::list<const Option*>& result) const
  {
    return this->_list(this->_root, path, result);
  }

  bool Configuration::list(const Option& root, const std::string path, std::list<const Option*>& result) const
  {
    return this->_list(root, path, result);
  }
  
  bool Configuration::list(const std::string path, std::list<std::string>& path_values) const
  {
    std::list<const Option*> values;
    if (this->list(path, values)) {
      for (const Option* chopt : values)
	path_values.push_back(chopt->value);
      return true;
    }
    return false;
  }

  bool Configuration::list(const Option& root, const std::string path, std::list<std::string>& path_values) const
  {
    std::list<const Option*> values;
    if (this->list(root, path, values)) {
      for (const Option* chopt : values)
	path_values.push_back(chopt->value);
      return true;
    }
    return false;
  }

  bool Configuration::_extract(const Option& root, const std::string& path, std::list<const Option*>& result) const
  {
    std::stringstream sspath(path);
    std::string sub_path;
    std::string part;
    std::string attr;
    std::string key;
    std::string value;
    this->_get_part(sspath, part, '/');
    std::getline(sspath, sub_path);
    std::stringstream sskv(part);
    std::getline(sskv, key, ':');
    this->_attr_key(attr, key);
    std::getline(sskv, value);
    bool attr_found = false;
    bool key_found = false;
    bool value_found = false;
    bool do_extract = false;
    if (attr.length() == 0) {
      attr_found = true;
    } else {
      if (attr == root.attr)
	attr_found = true;
    }
    if (key.length() == 0) {
      key_found = true;
    } else {
      if (key == root.key) {
	key_found = true;
      } else if (key[0] == '$' && key.substr(1, key.size() - 1) == root.key) {
	key_found = true;
	do_extract = true;
      }
    }
    if (value.length() == 0) {
      value_found = true;
    } else {
      if (value == root.value)
	value_found = true;
    }
    if (attr_found && key_found && value_found) {

      if (root.options.size() == 0 && sub_path.size() == 0) {
	if (do_extract) {
	  result.push_back(&root);
	}
	return true;
      }

      bool found = false;
      std::list<const Option*> tmp_result;
      for (const Option& chopt : root.options) {
	tmp_result.clear();
	if (this->_extract(chopt, sub_path, tmp_result)) {
	  found = true;
	  if (do_extract) {
	    result.push_back(&root);
	  }
	  if (tmp_result.size() > 0) {
	    for (const Option* chopt : tmp_result) {
	      result.push_back(chopt);
	    }
	  }      
	}
      }
	  
      return found;
    }
    return false;
  }

  bool Configuration::extract(const std::string path, std::list<const Option*>& result) const
  {
    return this->_extract(this->_root, path, result);
  }

  bool Configuration::extract(const Option& root, const std::string path, std::list<const Option*>& result) const
  {
    return this->_extract(root, path, result);
  }
  
  bool Configuration::extract(const std::string path, std::list<std::string>& path_values) const
  {
    std::list<const Option*> values;
    if (this->extract(path, values)) {
      for (const Option* chopt : values)
	path_values.push_back(chopt->value);
      return true;
    }
    return false;
  }

  bool Configuration::extract(const Option& root, const std::string path, std::list<std::string>& path_values) const
  {
    std::list<const Option*> values;
    if (this->extract(root, path, values)) {
      for (const Option* chopt : values)
	path_values.push_back(chopt->value);
      return true;
    }
    return false;
  }

  bool Configuration::get(const std::string path, const Option* value) const
  {
    std::list<const Option*> values;
    if (this->list(path, values)) {
      for (const Option* chopt : values) {
	value = chopt;
	break;
      }
      return true;
    }
    return false;
  }

  bool Configuration::get(const Option& root, const std::string path, const Option* value) const
  {
    std::list<const Option*> values;
    if (this->list(root, path, values)) {
      for (const Option* chopt : values) {
	value = chopt;
	break;
      }
      return true;
    }
    return false;
  }

  bool Configuration::get(const std::string path, std::string& value) const
  {
    std::list<const Option*> values;
    if (this->list(path, values)) {
      for (const Option* chopt : values) {
	value = chopt->value;
	break;
      }
      return true;
    }
    return false;
  }

  bool Configuration::get(const Option& root, const std::string path, std::string& value) const
  {
    std::list<const Option*> values;
    if (this->list(root, path, values)) {
      for (const Option* chopt : values) {
	value = chopt->value;
	break;
      }
      return true;
    }
    return false;
  }

  void Configuration::_attr_key(std::string& attr, std::string& key) const
  {
    if (key.length() > 0 && key[0] == '{') {
      for (size_t i = 1; i < key.length(); i++) {
	if (key[i] == '}') {
	  std::string tmp_key = "";
	  for (size_t j = i + 1; j < key.length(); j++)
	    tmp_key.push_back(key[j]);
	  key = tmp_key;
	  break;
	}
	attr.push_back(key[i]);
      }
    }
  }
	
  void Configuration::_load(Option& parent, int& line_no, int p_level)
  {
    std::string pure_line, line;
    std::streampos pos = this->_file->tellg();

    while (!this->_file->eof() && std::getline(*this->_file, pure_line)) {

      line_no++;
      line.clear();
      char prev_ch = '\0';
      bool comment = false;

      for (auto ch : pure_line) {
	if (!comment) {
	  if (ch == '#' && prev_ch != '\\') {
	    comment = true;
	  } else if (ch == '#' && prev_ch == '\\') {
	    line.pop_back();
	    line.push_back(ch);
	  } else {
	    line.push_back(ch);
	  }
	  prev_ch = ch;
	}
      }

      auto split = this->_split(line, line_no, p_level);
      int level = std::get<0>(split);
      
      if (level == -1) {

      } else if (level == p_level + 1) {
	std::string attr = "";
	std::string key = std::get<1>(split);
	std::string value = std::get<2>(split);
	this->_attr_key(attr, key);
	Option opt(attr, key, value);
	this->_load(opt, line_no, level);
	parent.options.push_back(opt);
      } else if (level <= p_level) {
	this->_file->seekg(pos);
	line_no--;
	return;
      } else {
	throw ConfigurationException("config indentation error", line_no, line);
      }
      pos = this->_file->tellg();
    }

    return;
  }

  void Configuration::load(std::istream& file)
  {
    int line_no = 0;
    this->_file = &file;
    this->_load(this->_root, line_no, -1);
  }

  void Configuration::load(const std::string file_path)
  {
    this->_file = new std::ifstream(file_path);
    if (!this->_file->good())
      throw ConfigurationException("file not found '" + file_path + "'");
    this->load(*this->_file);
    delete this->_file;
  }

  std::tuple<int, std::string, std::string> Configuration::_split(std::string line, int line_no, int p_level)
  {
    if (line.length() == 0)
      return std::make_tuple(-1, "", "");
    int level = 0;
    std::string key;
    std::string word;
    auto iter = line.begin();

    while (iter != line.end()) {

      if (*iter == '\t' && word.length() == 0) {
	level++;
	iter++;
	continue;
      } else if (*iter == ' ' && key.length() == 0) {
	throw ConfigurationException("key error", line_no, line);
      } else if (*iter == '/' && key.length() == 0) {
	throw ConfigurationException("key error", line_no, line);
      } else if (*iter == ':' && key.length() == 0 && word.length() == 0) {
	throw ConfigurationException("key error", line_no, line);
      } else if (*iter == ':' && word.length() > 0) {
	if (key.length() == 0) {
	  key = word;
	  word.clear();	
	  iter++;
	  continue;
	}
      }
      word.push_back(*iter);
      iter++;	
    }

    return std::make_tuple(level, key, Utility::trim(word));
  }

  void Configuration::_print(const Option& opt, int lvl) const
  {
    if (opt.options.size() > 0) {
      if (lvl > 0)
	std::cout << std::string( (lvl - 1) * 2, ' ');
      std::cout << '-';
    } else {
      std::cout << std::string(lvl * 2, ' ');
    }
    std::cout << opt.key << ": " << opt.value << std::endl;
    for (auto iter = opt.options.begin(); iter != opt.options.end(); iter++) {
      this->_print(*iter, lvl + 1);
    }
  }

  void Configuration::print() const
  {
    for (auto iter = this->_root.options.begin(); iter != this->_root.options.end(); iter++) {
      this->_print(*iter, 1);
    }
  }

}
