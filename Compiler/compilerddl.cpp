#include "compiler.hpp"
#include "compileexception.hpp"
#include "utility.hpp"
#include "shahelper.hpp"
#include <sstream>


namespace Zigurat
{

  // An object's qualified name split the way a permission is written: the
  // schema levels first, the object name last. "DEMO::AUTHORS" becomes
  // {"DEMO", "AUTHORS"}, and a permission covers the object when it is a
  // prefix of that.
  static std::vector<std::string> name_path(const std::string& type_name)
  {
    std::vector<std::string> path;

    size_t begin = 0;
    while (begin <= type_name.size()) {
      size_t end = type_name.find("::", begin);
      if (end == std::string::npos) end = type_name.size();
      const std::string level = type_name.substr(begin, end - begin);
      if (!level.empty()) path.push_back(level);
      if (end == type_name.size()) break;
      begin = end + 2;
    }

    return path;
  }

  // PATH: in the catalogue entry, so the same answer is readable without
  // loading the object.
  static void path_conf(std::stringstream& conf, const std::string& tab1, const std::string& tab2,
			const std::string& type_name)
  {
    conf << tab1 << "PATH:" << std::endl;
    for (const std::string& level : name_path(type_name))
      conf << tab2 << "LEVEL: " << level << std::endl;
  }

  // The text of a BEGIN HPP or BEGIN CPP block.
  //
  // The tokenizer wrapped it in the raw string literal every quoted Parsi
  // string is wrapped in -- R"ZIP0ML0S0( ... )ZIP0ML0S0" -- because that is the
  // one shape the rest of the compiler already carries. Twelve characters off
  // the front and eleven off the back is that wrapper and nothing else.
  static std::string block_text(const Expression& ast)
  {
    const std::string& raw = ast.args[0].token.value;
    if (raw.size() < 23) return std::string();
    return raw.substr(12, raw.size() - 23);
  }

  // INCLUDE and LINK written inside an object body belong to that object.
  //
  // File-scope clauses keep accumulating across the suite -- one INCLUDE above
  // three classes still gives the header to all three, which is what every demo
  // relies on. A body's clauses are added on top of whatever the file has
  // gathered so far and taken back off once the object is built, so the next
  // object in the same file does not inherit them. Which matters because each
  // object compiles standalone into its own .so: a model's -ltorch is the
  // model's, not its neighbour's.
  namespace
  {
    struct ClauseScope
    {
      std::list<std::string>& includes;
      std::list<std::string>& links;
      std::list<std::string>& compiles;
      const size_t include_mark;
      const size_t link_mark;
      const size_t compile_mark;

      ClauseScope(std::list<std::string>& incs, std::list<std::string>& lnks,
		  std::list<std::string>& cmps)
	: includes(incs), links(lnks), compiles(cmps),
	  include_mark(incs.size()), link_mark(lnks.size()), compile_mark(cmps.size())
      { }

      ~ClauseScope()
      {
	while (this->includes.size() > this->include_mark) this->includes.pop_back();
	while (this->links.size() > this->link_mark) this->links.pop_back();
	while (this->compiles.size() > this->compile_mark) this->compiles.pop_back();
      }
    };
  }

  void Compiler::_body_clauses(const Expression& ast)
  {
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "INCLUDE") {
	this->_include(expr);
      } else if (expr.token.value == "LINK") {
	this->_link(expr);
      } else if (expr.token.value == "COMPILE") {
	this->_compile_flag(expr);
      }
    }
  }

  void Compiler::_include(const Expression& ast)
  {
    this->_includes.push_back(ast.args[0].token.value.substr(1, ast.args[0].token.value.size() - 2));
  }

  void Compiler::_link(const Expression& ast)
  {
    this->_links.push_back(ast.args[0].token.value.substr(1, ast.args[0].token.value.size() - 2));
  }

  // A flag for the C++ compile rather than for the link. The two were never
  // interchangeable -- -std=c++17 and -I reach the compiler, and putting them
  // in LINK put them after the object file where they do nothing -- so an
  // object needing either had to have them added to the server's global
  // COMPILER/CPP_FLAGS and taken out again afterwards.
  void Compiler::_compile_flag(const Expression& ast)
  {
    this->_compiles.push_back(ast.args[0].token.value.substr(1, ast.args[0].token.value.size() - 2));
  }

  std::vector<Compiler::index_desc_t> Compiler::_table_indexes(const Expression& ast)
  {
    std::string full_name = this->_full_name(ast.args[0]);
    std::vector<index_desc_t> indexes;

    // A KEY'S TYPE IS THE WHOLE TYPE NAME, not the token that starts it:
    // `Vector<Double>' is one key type, and the token alone (`Vector') would
    // instantiate BTreeIndex<T, Vector> -- which is not a type. For a
    // plain type the name IS the token, so nothing else changes.
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	if (expr.args.size() > 2) {
	  if (expr.args[2].token.value == "PRIMARY") {
	    indexes.emplace_back(std::vector<std::string>{expr.args[0].token.value}, std::vector<std::string>{this->_type_name(expr.args[1])},
				 expr.args[2].token.value, "IDX_" + full_name + "_" + expr.args[0].token.value, true);
	  } else if (expr.args[2].token.value == "UNIQUE") {
	    indexes.emplace_back(std::vector<std::string>{expr.args[0].token.value}, std::vector<std::string>{this->_type_name(expr.args[1])},
				 expr.args[2].token.value, "IDX_" + full_name + "_" + expr.args[0].token.value, true);
	  } else if (expr.args[2].token.value == "INDEX") {
	    indexes.emplace_back(std::vector<std::string>{expr.args[0].token.value}, std::vector<std::string>{this->_type_name(expr.args[1])},
				 expr.args[2].token.value, "IDX_" + full_name + "_" + expr.args[0].token.value, false);
	  }
	}
      }
    }

    auto columns_types([&] (const Expression& expr, const std::vector<std::string>& columns) -> std::vector<std::string> {
	std::vector<std::string> types;
	for (const std::string& column : columns) {
	  bool found = false;
	  for (const Expression& expr : ast.args) {
	    if (expr.token.value == "COLUMN" && expr.args[0].token.value == column) {
	      types.push_back(this->_type_name(expr.args[1]));
	      found = true;
	    }
	  }
	  if (!found)
	    throw CompileException("undefined column '" + column + "'", expr);
	}
	return types;
      });

    for (const Expression& expr : ast.args) {
      
      if (expr.token.value == "PRIMARY" || expr.token.value == "UNIQUE" || expr.token.value == "INDEX") {
	
	for (const index_desc_t& index : indexes) {
	  if (expr.token.value == "PRIMARY" && std::get<2>(index) == "PRIMARY") {
	    throw CompileException("primary key redefinition", expr); 
	  }
	}

	bool has_name = false;
	const Expression* name_expr = nullptr;
	if (!expr.args.empty() && expr.args[0].args.empty()) { // index has not name
	  name_expr = &expr;
	} else {
	  has_name = true;
	  name_expr = &(expr.args[0]);
	}

	std::vector<std::string> columns;
	for (const Expression& ch_expr : name_expr->args) {
	  columns.push_back(ch_expr.token.value);
	}
	
	std::string index_name;
	if (has_name) { // index has not name
	  index_name = name_expr->token.value;
	} else {
	  index_name = "IDX_" + full_name;
	  for (const std::string& column : columns)
	    index_name += "_" + column;
	}

	auto columns_check([&] (const std::vector<std::string>& columns_1, const index_desc_t& index_2) -> bool {
	    size_t count_1 = columns_1.size();
	    size_t count_2 = std::get<0>(index_2).size();
	    
	    if (count_1 == count_2) {
	      for (const std::string& column_1 : columns_1) {
		for (const std::string& column_2 : std::get<0>(index_2)) {
		  if (column_1 == column_2) {
		    count_1--;
		  }
		}
	      }
	      if (count_1 == 0)
		return true;
	      return false;
	    } else {
	      return false;
	    }
	  });

	for (const index_desc_t& index : indexes) {
	  if (columns_check(columns, index))
	    throw CompileException("index redefinition", expr);
	  else if (index_name == std::get<3>(index))
	    throw CompileException("index name redefinition'" + index_name + "'", expr); 
	}

	indexes.emplace_back(columns, columns_types(expr, columns), expr.token.value, index_name, 
			     expr.token.value == "PRIMARY" || expr.token.value == "UNIQUE");
      }

    }

    return indexes;
  }
  
  void Compiler::_table(const Expression& ast)
  {
    std::string name = this->_name(ast.args[0]);
    std::string full_name = this->_full_name(ast.args[0]);
    std::string type_name = this->_type_name(ast.args[0]);
    std::string guard_name = this->_guard_name(ast.args[0]);
    std::string include_name = this->_include_name(ast.args[0]);
    std::string hash_key = SHA::checksum(SHA::SHA1, type_name);
    std::stringstream head;
    std::stringstream impl;
    std::stringstream conf;
    std::list<std::string> requires;
    
    // header file

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;

    // THE CICILI ENGINE'S CONSUMER SURFACE, and nothing of the old
    // MVCCS: the two engines never meet in one translation unit.
    head << "#include \"engine-compat.hpp\"" << std::endl;
    
    if (ast.args.size() > 1 && ast.args[2].token.value == "REQUIRES") {
      for (const Expression& expr : ast.args[1].args) {
	std::string inc_name = this->_include_name(expr);
	requires.push_back(inc_name);
	head << "#include \"" << inc_name << ".hpp\"" << std::endl;
      }
    }

    this->_open_namespace(ast.args[0], head);
    head << "class " << name << " : public BaseTable" << std::endl;
    head << "{" << std::endl;
    head << "private:" << std::endl;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	head << TAB1 << this->_type_name(expr.args[1]) << " _" << expr.args[0].token.value;
	head <<';' << std::endl;
      }
    }

    head << "public:" << std::endl;
    head << TAB1 << "static std::string name;" << std::endl;
    head << TAB1 << "static std::vector<std::string> path;" << std::endl;
    head << TAB1 << "static Zigurat::hashkey_t hash_key;" << std::endl;
    head << TAB1 << name << "();" << std::endl;
    
    bool has_column = false;
    head << TAB1 << name << '('; // Default Insert Constructor
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	head << this->_type_name(expr.args[1]);
	if (expr.args[expr.args.size() - 1].token.value == "DEFAULT") {
	  head << " = ";
	  this->_expr.compile(expr.args[expr.args.size() - 1].args[0], head );
	}
        head << ", ";
	has_column = true;
      }
    }
    if (has_column)
      head.seekp(-2, std::ios::cur);
    head << ");" << std::endl;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	head << TAB1 << "typedef " << this->_type_name(expr.args[1]) << ' ' << name << "::*" << expr.args[0].token.value << "_t;" << std::endl;
      }
    }
    
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	head << TAB1 << "static " << expr.args[0].token.value << "_t " << expr.args[0].token.value << ';' << std::endl;
      }
    }
    
    std::vector<index_desc_t> indexes = this->_table_indexes(ast);
    for (const index_desc_t& index : indexes) {
      head << TAB1 << "static Zigurat::BTreeIndex<" << name << ", ";
      for (const std::string& column : std::get<1>(index))
	head << column << ", ";
      head.seekp(-2, std::ios::cur);
      head << "> " << std::get<3>(index) << ';' << std::endl;
    }
    // each index's own 20-byte identity, and the attach that fills the
    // engine-side records lazily -- a static initializer may not touch
    // the store, because parsic loads table objects with no store open
    for (const index_desc_t& index : indexes) {
      head << TAB1 << "static uint8_t " << std::get<3>(index) << "_keybytes[20];" << std::endl;
      if (std::get<0>(index).size() > 1)
	head << TAB1 << "static uint8_t " << std::get<3>(index) << "_depbytes[20];" << std::endl;
    }
    head << TAB1 << "static void attach_indexes();" << std::endl;

    head << TAB1 << "int64_t pack_size() override;" << std::endl;
    head << TAB1 << "void prepare() override;" << std::endl;
    head << TAB1 << "void pack(Zigurat::binarystream&) override;" << std::endl;
    head << TAB1 << "void unpack(Zigurat::binarystream&) override;" << std::endl;
    head << TAB1 << "void map(void*) override;" << std::endl;
    head << TAB1 << "void unmap(void*) override;" << std::endl;
    // Shadows BaseTable::truncate_indexes, so Memory::truncate<T>() reaches
    // this table's indexes rather than the empty one on the base.
    head << TAB1 << "static void truncate_indexes();" << std::endl;
    head << TAB1 << "friend Zigurat::binarystream& operator<<(Zigurat::binarystream&, const " << name << "&);" << std::endl;
    head << TAB1 << "friend Zigurat::binarystream& operator>>(Zigurat::binarystream&, " << name << "&);"  << std::endl;
    head << "};" << std::endl;
    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;

    // implementation file

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;
    this->_open_namespace(ast.args[0], impl);
    impl << "std::string " << name << "::name = \"" << type_name << "\";" << std::endl;
    impl << "std::vector<std::string> " << name << "::path = {";
    {
      const std::vector<std::string> path = name_path(type_name);
      for (size_t i = 0; i < path.size(); i++)
	impl << ((i > 0) ? ", " : "") << "\"" << path[i] << "\"";
    }
    impl << "};" << std::endl;
    impl << "Zigurat::hashkey_t " << name << "::hash_key = {";
    for (size_t i = 0; i < hash_key.size(); i+=2)
      impl << "0x" << hash_key[i] << hash_key[i + 1] << ',';
    impl.seekp(-1, std::ios::cur);
    impl << "};" << std::endl;
    
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
        impl << name << "::" << expr.args[0].token.value << "_t " << name << "::" << expr.args[0].token.value 
	     << " = &" << name << "::_" << expr.args[0].token.value << ';' << std::endl;
      }
    }
    
    for (const index_desc_t& index : indexes) {
      impl << "Zigurat::BTreeIndex<" << name << ", ";
      for (const std::string& column : std::get<1>(index))
        impl << column << ", ";
      impl.seekp(-2, std::ios::cur);
      impl << "> " << name << "::" << std::get<3>(index) << ";" << std::endl;

      // the index's identity: SHA-1 of its qualified spelling, exactly
      // as the table's own hash key is derived
      std::string index_key = SHA::checksum(SHA::SHA1, type_name + "::" + std::get<3>(index));
      impl << "uint8_t " << name << "::" << std::get<3>(index) << "_keybytes[20] = {";
      for (size_t i = 0; i < index_key.size(); i += 2)
        impl << "0x" << index_key[i] << index_key[i + 1] << ',';
      impl.seekp(-1, std::ios::cur);
      impl << "};" << std::endl;
      if (std::get<0>(index).size() > 1) {
	std::string dep_key = SHA::checksum(SHA::SHA1, type_name + "::" + std::get<3>(index) + "~dep");
	impl << "uint8_t " << name << "::" << std::get<3>(index) << "_depbytes[20] = {";
	for (size_t i = 0; i < dep_key.size(); i += 2)
	  impl << "0x" << dep_key[i] << dep_key[i + 1] << ',';
	impl.seekp(-1, std::ios::cur);
	impl << "};" << std::endl;
      }
    }

    // attach: fill the engine-side index records ONCE, lazily and
    // thread-safely (a C++ magic static), the first time any data
    // operation or index cursor needs them
    impl << "void " << name << "::attach_indexes()" << std::endl;
    impl << '{' << std::endl;
    impl << TAB1 << "static bool engine_attached = ([] () -> bool {" << std::endl;
    impl << TAB2 << "Memory* m = ::globals_memory();" << std::endl;
    for (const index_desc_t& index : indexes) {
      const std::string& iname = std::get<3>(index);
      impl << TAB2 << "{" << std::endl;
      impl << TAB3 << "::BTreeIndex* bt = &" << name << "::" << iname << ".bt;" << std::endl;
      impl << TAB3 << "bt->m = m;" << std::endl;
      impl << TAB3 << "bt->name = \"" << iname << "\";" << std::endl;
      impl << TAB3 << "bt->hash_key = intern_key(" << name << "::" << iname << "_keybytes);" << std::endl;
      impl << TAB3 << "bt->table_key = " << name << "::hash_key;" << std::endl;
      impl << TAB3 << "bt->catalogue_id = engine_key64_bytes_fold(" << name << "::" << iname << "_keybytes);" << std::endl;
      impl << TAB3 << "bt->is_unique = " << ((std::get<4>(index)) ? 1 : 0) << ";" << std::endl;
      impl << TAB3 << "bt->branching_factor = 65;" << std::endl;
      impl << TAB3 << "bt->min_degree = 64;" << std::endl;
      impl << TAB3 << "bt->max_degree = 128;" << std::endl;
      impl << TAB3 << "bt->root_address = -1;" << std::endl;
      impl << TAB3 << "bt->record_pointer = pointer_null();" << std::endl;
      impl << TAB3 << "bt->levels = " << std::get<0>(index).size() << ";" << std::endl;
      impl << TAB3 << "bt->is_dependent = 0;" << std::endl;
      if (std::get<0>(index).size() > 1)
	impl << TAB3 << "bt->dep_hash_key = intern_key(" << name << "::" << iname << "_depbytes);" << std::endl;
      else
	impl << TAB3 << "bt->dep_hash_key = nullptr;" << std::endl;
      impl << TAB3 << "bt_select_record(bt);" << std::endl;
      impl << TAB2 << "}" << std::endl;
    }
    impl << TAB2 << "return true;" << std::endl;
    impl << TAB1 << "})();" << std::endl;
    impl << TAB1 << "(void)engine_attached;" << std::endl;
    impl << '}' << std::endl;

    // the ensure pointers: a static initializer that touches no store,
    // so an index cursor's first use attaches before it walks
    impl << "static struct " << name << "_engine_registrar_t {" << std::endl;
    impl << TAB1 << name << "_engine_registrar_t() {" << std::endl;
    for (const index_desc_t& index : indexes)
      impl << TAB2 << name << "::" << std::get<3>(index) << ".ensure = &" << name << "::attach_indexes;" << std::endl;
    impl << TAB1 << "}" << std::endl;
    impl << "} _" << name << "_engine_registrar;" << std::endl;

    impl << name << "::" << name << "()" << std::endl; // Default Constructor
    impl << " : ";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
        impl << "_" << expr.args[0].token.value << "(nullptr)" << ", ";
      }
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << std::endl;
    impl << '{' << std::endl;
    impl << '}' << std::endl;

    impl << name << "::" << name << '('; // Default Insert Constructor
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	impl << this->_type_name(expr.args[1]) << ' ' << expr.args[0].token.value << ", ";
      }
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << ')' << std::endl;
    impl << " : ";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	impl << '_' << expr.args[0].token.value << '(' << expr.args[0].token.value << "), ";
      }
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << std::endl;
    impl << '{' << std::endl;
    impl << '}' << std::endl;

    impl << "void " << name << "::prepare()" << std::endl; // Check consistency
    impl << '{' << std::endl;
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	for (const Expression& ch_expr : expr.args) {
	  if (ch_expr.token.value == "NOT") {
	    impl << TAB1 << "if (this->_" << expr.args[0].token.value 
		 << ".is_null()) throw Zigurat::ZiguratException(7700, \"not null column '" 
		 << expr.args[0].token.value << "'\");" << std::endl;
	  } else if (ch_expr.token.value == "PRIMARY") {
	    impl << TAB1 << "if (this->_" << expr.args[0].token.value 
		 << ".is_null()) throw Zigurat::ZiguratException(7700, \"not null column '" 
		 << expr.args[0].token.value << "'\");" << std::endl;
	  } else if (ch_expr.token.value == "DEFAULT") {
	    impl << TAB1 << "if (this->_" << expr.args[0].token.value 
		 << ".is_null()) this->_" << expr.args[0].token.value << " = ";
	    this->_expr.compile(ch_expr.args[0], impl);
	    impl << ';' << std::endl;
	  }
        }
      }
    }
    impl << '}' << std::endl;

    auto emit_keys([&] (const index_desc_t& index, const char* fn) {
	if (std::get<0>(index).size() == 1) {
	  impl << TAB1 << fn << "(&" << name << "::" << std::get<3>(index)
	       << ".bt, engine_key64(this->_" << std::get<0>(index)[0]
	       << "), this->pointer.address);" << std::endl;
	} else {
	  impl << TAB1 << "{ int64_t ks[8];";
	  const std::vector<std::string>& cols = std::get<0>(index);
	  for (size_t i = 0; i < cols.size(); i++)
	    impl << " ks[" << i << "] = engine_key64(this->_" << cols[i] << ");";
	  impl << " " << fn << "_multi(&" << name << "::" << std::get<3>(index)
	       << ".bt, ks, this->pointer.address); }" << std::endl;
	}
      });

    impl << "void " << name << "::map(void*)" << std::endl; // engine bt_map
    impl << '{' << std::endl;
    if (!indexes.empty())
      impl << TAB1 << name << "::attach_indexes();" << std::endl;
    for (const index_desc_t& index : indexes)
      emit_keys(index, "bt_map");
    impl << '}' << std::endl;

    impl << "void " << name << "::unmap(void*)" << std::endl; // engine bt_unmap
    impl << '{' << std::endl;
    if (!indexes.empty())
      impl << TAB1 << name << "::attach_indexes();" << std::endl;
    for (const index_desc_t& index : indexes)
      emit_keys(index, "bt_unmap");
    impl << '}' << std::endl;

    // a truncate is the one moment the table is at its smallest: drop
    // every index's storage wholesale, then one scan re-maps the
    // survivors into fresh trees
    impl << "void " << name << "::truncate_indexes()" << std::endl;
    impl << '{' << std::endl;
    if (!indexes.empty()) {
      impl << TAB1 << name << "::attach_indexes();" << std::endl;
      impl << TAB1 << "Memory* m = ::globals_memory();" << std::endl;
      for (const index_desc_t& index : indexes)
	impl << TAB1 << "bt_drop_storage(&" << name << "::" << std::get<3>(index) << ".bt, m);" << std::endl;
      impl << TAB1 << "Globals::memory()->cursor<" << name << ">([] (" << name << "& r) -> bool { r.map(nullptr); return true; });" << std::endl;
    }
    impl << '}' << std::endl;

    impl << "int64_t " << name << "::pack_size()" << std::endl; // Pack Size
    impl << "{" << std::endl;
    impl << TAB1 << "return Zigurat::binarystream::pack_size(";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
        impl << "this->_" << expr.args[0].token.value << ", ";
	has_column = true;
      }
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << ");" << std::endl;
    impl << '}' << std::endl;

    // the virtuals the Cicili engine dispatches -- the SAME variadic
    // pack the operators below use, so a row's bytes are identical to
    // what the C++ engine wrote and a store carries over
    impl << "void " << name << "::pack(Zigurat::binarystream& io)" << std::endl;
    impl << '{' << std::endl;
    impl << TAB1 << "io.pack(";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN")
        impl << "this->_" << expr.args[0].token.value << ", ";
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << ");" << std::endl;
    impl << '}' << std::endl;

    impl << "void " << name << "::unpack(Zigurat::binarystream& io)" << std::endl;
    impl << '{' << std::endl;
    impl << TAB1 << "io.unpack(";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN")
        impl << "this->_" << expr.args[0].token.value << ", ";
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << ");" << std::endl;
    impl << '}' << std::endl;

    impl << "Zigurat::binarystream& operator<<(Zigurat::binarystream& outstream, const " << name << "& o)" << std::endl;
    impl << '{' << std::endl;
    impl << TAB1 << "outstream.pack(";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
        impl << "o._" << expr.args[0].token.value << ", ";
      }
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << ");" << std::endl;
    impl << TAB1 << "return outstream;" << std::endl;
    impl << '}' << std::endl;

    impl << "Zigurat::binarystream& operator>>(Zigurat::binarystream& instream, " << name << "& o)" << std::endl;
    impl << '{' << std::endl;
    impl << TAB1 << "instream.unpack(";
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
        impl << "o._" << expr.args[0].token.value << ", ";
      }
    }
    if (has_column)
      impl.seekp(-2, std::ios::cur);
    impl << ");" << std::endl;
    impl << TAB1 << "return instream;" << std::endl;
    impl << '}' << std::endl;

    this->_close_namespace(ast.args[0], impl);

    // config file

    conf << "TABLE:" << std::endl;
    conf << TAB1 << "NAME: " << name << std::endl;
    conf << TAB1 << "DOMAIN: " << this->_domain(ast.args[0]) << std::endl;
    conf << TAB1 << "FULL_NAME: " << full_name << std::endl;
    conf << TAB1 << "TYPE_NAME: " << type_name << std::endl;
    path_conf(conf, TAB1, TAB2, type_name);
    conf << TAB1 << "GUARD_NAME: " << guard_name << std::endl;
    conf << TAB1 << "HASH_KEY: " << hash_key << std::endl;
    conf << TAB1 << "REQUIRES:" << std::endl;
    for (const std::string& inc : requires) {
      conf << TAB2 << "REQUIRE: " << inc << std::endl;
    }
    conf << TAB1 << "COLUMNS:" << std::endl;
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	conf << TAB2 << "COLUMN:" << std::endl;
        conf << TAB3 << "NAME: " << expr.args[0].token.value << std::endl;
        conf << TAB3 << "TYPE: " << this->_type_name(expr.args[1]) << std::endl;
	for (const Expression& ch_expr : expr.args) {
	  if (ch_expr.token.value == "NOT") {
	    conf << TAB3 << "NULLABLE: FALSE" << std::endl;
	    break;
	  } else if (ch_expr.token.value == "NULL") {
	    conf << TAB3 << "NULLABLE: TRUE" << std::endl;
	    break;
	  }
	}
	if (expr.args[expr.args.size() - 1].token.value == "DEFAULT") {
	  conf << TAB3 << "DEFAULT: ";
	  this->_expr.compile(expr.args[expr.args.size() - 1].args[0], conf);
	  conf << std::endl;
	}
      }
    }
    conf << TAB1 << "KEYS:" << std::endl;
    for (const index_desc_t& index : indexes) {
      conf << TAB2 << "KEY:" << std::endl;
      conf << TAB3 << "TYPE: " << std::get<2>(index) << std::endl;
      conf << TAB3 << "NAME: " << std::get<3>(index) << std::endl;
      conf << TAB3 << "IS_UNIQUE: " << ((std::get<4>(index)) ? "TRUE" : "FALSE") << std::endl;
      conf << TAB3 << "COLUMNS:" << std::endl;
      for (const std::string& column : std::get<0>(index))
	conf << TAB4 << "COLUMN: " << column << std::endl;
      conf << TAB3 << "TYPES:" << std::endl;
      for (const std::string& type : std::get<1>(index))
	conf << TAB4 << "TYPE: " << type << std::endl;
    }
        
    // the same table again, as one .cicili for the Cicili MVCCS
    {
      std::stringstream cicili;
      cicili << ";;;; " << type_name << " -- generated by the Parsi compiler beside the" << std::endl;
      cicili << ";;;; C++ pair. One file is header and source both: import it into a" << std::endl;
      cicili << ";;;; target that imports the Cicili MVCCS engine (MVCCS-cicili/), whose" << std::endl;
      cicili << ";;;; def... macros expand the object. Attach each index once, after" << std::endl;
      cicili << ";;;; memory_open." << std::endl;
      cicili << ";;;;" << std::endl;
      cicili << ";;;; Columns, with their Parsi types (Cicili MVCCS columns are int64," << std::endl;
      cicili << ";;;; STRING/TEXT ride as TEXT, FLOAT/DOUBLE/REAL as REAL, Vector<Double>" << std::endl;
      cicili << ";;;; as a VECTOR of doubles). Every index is a defindex: an INT or REAL" << std::endl;
      cicili << ";;;; key keeps its order, a TEXT or VECTOR key is a hash -- equality" << std::endl;
      cicili << ";;;; only, the row re-checked by the consumer:" << std::endl;
      for (const Expression& expr : ast.args) {
	if (expr.token.value == "COLUMN")
	  cicili << ";;;;   " << expr.args[0].token.value << " " << this->_type_name(expr.args[1]) << std::endl;
      }
      cicili << std::endl;

      // an imported .cicili is evaluated Lisp, so the object ships as a
      // macro: the importing target says (define-NAME) where its forms go
      cicili << "(DEFMACRO define-" << full_name << " ()" << std::endl;
      cicili << "  '($$$" << std::endl;
      // the qualified spelling is the name AND the SQL name: Cicili's ::
      // is a name of its own, so no string rides beside it
      cicili << "    (deftable " << type_name << "";
      for (const Expression& expr : ast.args) {
	if (expr.token.value == "COLUMN") {
	  const std::string col_type = this->_type_name(expr.args[1]);
	  // STRING/TEXT columns ride as std::string members, the floating
	  // family as a double, Vector columns as the engine's dvec_t (count
	  // + doubles); everything else is the engine's int64 column
	  if (col_type == "STRING" || col_type == "TEXT")
	    cicili << " (TEXT " << expr.args[0].token.value << ")";
	  else if (col_type == "FLOAT" || col_type == "DOUBLE" || col_type == "REAL")
	    cicili << " (REAL " << expr.args[0].token.value << ")";
	  else if (col_type.rfind("VECTOR", 0) == 0)
	    cicili << " (VECTOR " << expr.args[0].token.value << ")";
	  else
	    cicili << " " << expr.args[0].token.value;
	}
      }
      cicili << ")" << std::endl;

      // branching 65 is the Long-key factor the C++ engine derives from the
      // key type's width; the Cicili engine takes it as a parameter. Every
      // index ships as a real defindex: the engine folds each key by its
      // column's kind (a STRING/TEXT or Vector key is its hash, equality
      // only) exactly as the C++ pair's engine_key64 does
      for (const index_desc_t& index : indexes) {
	cicili << "    (defindex " << std::get<3>(index) << " " << type_name << " ";
	const std::vector<std::string>& columns = std::get<0>(index);
	if (columns.size() == 1) {
	  cicili << columns[0];
	} else {
	  cicili << "(";
	  for (size_t i = 0; i < columns.size(); i++)
	    cicili << ((i > 0) ? " " : "") << columns[i];
	  cicili << ")";
	}
	cicili << " " << (std::get<4>(index) ? 1 : 0) << " 65)" << std::endl;
      }
      cicili << "    ))" << std::endl;

      this->_cicili_file(include_name, cicili.str());
    }

    this->_build(include_name, requires, head, impl, conf, ast, type_name, true);
  }

  void Compiler::_parameters_head(const Expression& ast, std::stringstream& code, int lvl)
  {
    bool has_param = false;
    code << '(';
    for (const Expression& expr : ast.args) {
      code << this->_type_name(expr.args[0]);
      if (expr.args.size() == 2) {
	if (expr.args[1].token.value == "$dir" && expr.args[1].args.size() > 0) {
	  if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	    code << "&";
	  }
        } else {
	  code << " = ";
	  this->_expr.compile(expr.args[1], code);
	}
      } else if (expr.args.size() == 3 && expr.args[1].args.size() > 0) {
	if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	  code << "&";
	}
        code << " = ";
	this->_expr.compile(expr.args[2], code);
      }
      code << ", ";
      has_param = true;
    }
    if (has_param)
      code.seekp(-2, std::ios::cur);
    code << ")";
  }
    
  void Compiler::_parameters_impl(const Expression& ast, std::stringstream& code, int lvl)
  {
    bool has_param = false;
    code << '(';
    for (const Expression& expr : ast.args) {
      code << this->_type_name(expr.args[0]);
      if (expr.args.size() == 2) {
	if (expr.args[1].token.value == "$dir" && expr.args[1].args.size() > 0) {
	  if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	    code << "&";
	  }
        }
      } else if (expr.args.size() == 3 && expr.args[1].args.size() > 0) {
	if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	  code << "&";
	}
      }
      code << ' ' << expr.token.value << ", ";
      has_param = true;
    }
    if (has_param)
      code.seekp(-2, std::ios::cur);
    code << ")";
  }
    
  void Compiler::_parameters_tmpl(const Expression& ast, std::stringstream& code, int lvl)
  {
    bool has_param = false;
    code << '(';
    for (const Expression& expr : ast.args) {
      code << this->_type_name(expr.args[0]);
      if (expr.args.size() == 2) {
	if (expr.args[1].token.value == "$dir" && expr.args[1].args.size() > 0) {
	  if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	    code << "&";
	  }
        } else {
	  code << " = ";
	  this->_expr.compile(expr.args[1], code);
	}
      } else if (expr.args.size() == 3 && expr.args[1].args.size() > 0) {
	if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	  code << "&";
	}
	code << " = ";
	this->_expr.compile(expr.args[2], code);
      }
      code << ' ' << expr.token.value << ", ";
      has_param = true;
    }
    if (has_param)
      code.seekp(-2, std::ios::cur);
    code << ")";
  }
    
  void Compiler::_procedure(const Expression& ast)
  {
    std::string name = this->_name(ast.args[0]);
    std::string full_name = this->_full_name(ast.args[0]);
    std::string type_name = this->_type_name(ast.args[0]);
    std::string guard_name = this->_guard_name(ast.args[0]);
    std::string include_name = this->_include_name(ast.args[0]);
    std::string hash_key = SHA::checksum(SHA::SHA1, type_name);
    std::stringstream head;
    std::stringstream impl;
    std::stringstream conf;
    std::list<std::string> requires;

    // Lives until _build has written the files, then puts the two lists back.
    ClauseScope clauses(this->_includes, this->_links, this->_compiles);
    this->_body_clauses(ast);

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;
    head << "#include \"engine-compat.hpp\"" << std::endl;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "REQUIRES") {
	for (const Expression& ch_expr : expr.args) {
	  std::string inc_name = this->_include_name(ch_expr);
	  requires.push_back(inc_name);
	  head << "#include \"" << inc_name << ".hpp\"" << std::endl;
	}
	break;
      }
    }

    // Above the namespace, so the block can carry its own headers -- the same
    // arrangement _class uses, and for the same reason.
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "HPP") {
	head << block_text(expr) << std::endl;
      }
    }

    this->_open_namespace(ast.args[0], head);

    head << TAB1 << this->_type_name(ast.args[2].args[0]);
    head << ' ' << name;
    this->_parameters_head(ast.args[1], head, 1);
    head << ";" << std::endl;

    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;

    head << "extern \"C\" void call();" << std::endl;

    // Before the header include, for the reason spelled out in _class: this
    // object's globals.hpp would otherwise have turned AUTO, CAST, VOID and
    // NULL into macros before the block's own headers were parsed.
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "CPP") {
	impl << block_text(expr) << std::endl;
      }
    }

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;

    this->_open_namespace(ast.args[0], impl);

    impl << TAB1 << this->_type_name(ast.args[2].args[0]);
    impl << ' ' << name;
    this->_parameters_impl(ast.args[1], impl, 1);
    impl << std::endl;
    impl << TAB1 << "{" << std::endl;

    const Expression* body = &ast.args[ast.args.size() - 1];
    for (const Expression& expr : body->args) {
      this->_clause(expr, impl, 2);
    }

    impl << TAB1 << "}" << std::endl;

    this->_close_namespace(ast.args[0], impl);

    // RPC

    impl << "extern \"C\" void call()" << std::endl;
    impl << '{' << std::endl;
    bool has_param = false;
    for (const Expression& expr : ast.args[1].args) {
      if (expr.args.size() == 1) {
	impl << TAB1 << this->_type_name(expr.args[0]) << ' ' << expr.token.value << "(nullptr);" << std::endl;
	impl << TAB1 << "*Globals::client_stream() >> " << expr.token.value << ';' << std::endl;
      } else if (expr.args.size() == 2) {
	if (expr.args[1].token.value == "$dir" && expr.args[1].args.size() > 0) {
	  impl << TAB1 << this->_type_name(expr.args[0]) << ' ' << expr.token.value << "(nullptr);" << std::endl;
	  if (expr.args[1].args[0].token.value == "IN" || expr.args[1].args[0].token.value == "INOUT") {
	    impl << TAB1 << "*Globals::client_stream() >> " << expr.token.value << ';' << std::endl;
	  }
	} else {
	  impl << TAB1 << this->_type_name(expr.args[0]) << ' ' << expr.token.value << " = ";
	  this->_expr.compile(expr.args[1], impl);
	  impl << ';' << std::endl;
	}
      } else if (expr.args.size() == 3 && expr.args[1].args.size() > 0) {
	impl << TAB1 << this->_type_name(expr.args[0]) << ' ' << expr.token.value << "(nullptr);" << std::endl;
	if (expr.args[1].args[0].token.value == "IN" || expr.args[1].args[0].token.value == "INOUT") {
	  impl << TAB1 << "*Globals::client_stream() >> " << expr.token.value << ';' << std::endl;
	}
	impl << TAB1 << this->_type_name(expr.args[0]) << ' ' << expr.token.value << " = ";
	this->_expr.compile(expr.args[2], impl);
	impl << ';' << std::endl;
      }
      has_param = true;
    }
    
    impl << TAB1;
    std::string obj = "o_" + std::to_string(std::rand());
    if (ast.args[2].args[0].token.value != "VOID") {
      impl << this->_type_name(ast.args[2].args[0]) << ' ' << obj << " = ";
    }
    impl << type_name << '(';
    for (const Expression& expr : ast.args[1].args)
      impl << expr.token.value << ", ";
    if (has_param)
      impl.seekp(-2, std::ios::cur);
    impl << ");" << std::endl;
    
    if (ast.args[2].args[0].token.value != "VOID") {
      impl << TAB1 << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::RETURN_VALUE);" << std::endl;
      impl << TAB1 << "*Globals::client_stream() << " << obj << ';' << std::endl;
    }
    for (const Expression& expr : ast.args[1].args) {
      if (expr.args.size() == 2) {
	if (expr.args[1].token.value == "$dir") {	  
	  if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	    impl << TAB1 << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::RETURN_VALUE);" << std::endl;
	    impl << TAB1 << "*Globals::client_stream() << " << expr.token.value << ';' << std::endl;
	  }
	}
      } else if (expr.args.size() == 3) {
	impl << TAB1 << this->_type_name(expr.args[0]) << ' ' << expr.token.value << ';' << std::endl;
	if (expr.args[1].args[0].token.value == "OUT" || expr.args[1].args[0].token.value == "INOUT") {
	  impl << TAB1 << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::RETURN_VALUE);" << std::endl;
	  impl << TAB1 << "*Globals::client_stream() << " << expr.token.value << ';' << std::endl;
	}
      }
    }
    impl << TAB1 << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::SUCCESSFUL_DONE);" << std::endl;
    
    impl << '}' << std::endl;

    // config file

    conf << "PROCEDURE:" << std::endl;
    conf << TAB1 << "NAME: " << name << std::endl;
    conf << TAB1 << "DOMAIN: " << this->_domain(ast.args[0]) << std::endl;
    conf << TAB1 << "FULL_NAME: " << full_name << std::endl;
    conf << TAB1 << "TYPE_NAME: " << type_name << std::endl;
    path_conf(conf, TAB1, TAB2, type_name);
    conf << TAB1 << "GUARD_NAME: " << guard_name << std::endl;
    conf << TAB1 << "HASH_KEY: " << hash_key << std::endl;
    conf << TAB1 << "PARAMETER:" << std::endl;
    for (const Expression& expr : ast.args[1].args) {
      conf << TAB2 << "PARAMETER:" << std::endl;
      conf << TAB3 << "NAME: " << expr.token.value << std::endl;
      conf << TAB3 << "TYPE: " << this->_type_name(expr.args[0]) << std::endl;
      if (expr.args.size() == 2) {
	if (expr.args[1].token.value == "$dir") {
	  conf << TAB3 << "DIRECTION: " << expr.args[1].token.value << std::endl;
	} else {
	  conf << TAB3 << "DEFAULT: ";
	  this->_expr.compile(expr.args[1], conf);
	  conf << std::endl;
	}
      } else if (expr.args.size() == 3) {
	conf << TAB3 << "DIRECTION: " << expr.args[1].token.value << std::endl;
	conf << TAB3 << "DEFAULT: ";
	this->_expr.compile(expr.args[2], conf);
	conf << std::endl;
      }
    }
    conf << TAB1 << "REQUIRES:" << std::endl;
    for (const std::string& inc : requires) {
      conf << TAB2 << "REQUIRE: " << inc << std::endl;
    }
    
    this->_build(include_name, requires, head, impl, conf, ast, type_name, true);
  }

  void Compiler::_class(const Expression& ast, bool is_page)
  {
    std::string name = this->_name(ast.args[0]);
    std::string full_name = this->_full_name(ast.args[0]);
    std::string type_name = this->_type_name(ast.args[0]);
    std::string guard_name = this->_guard_name(ast.args[0]);
    std::string include_name = this->_include_name(ast.args[0]);
    std::string hash_key = SHA::checksum(SHA::SHA1, type_name);
    std::stringstream head;
    std::stringstream impl;
    std::stringstream conf;
    std::list<std::string> requires;
    bool is_tmpl_class = type_name.back() == '>';
    
    if (is_tmpl_class && is_page)
      throw CompileException("pages couldn't be template", ast);

    // Lives until _build has written the files, then puts the two lists back.
    ClauseScope clauses(this->_includes, this->_links, this->_compiles);
    this->_body_clauses(ast);

    // header file

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;

    head << "#include \"engine-compat.hpp\"" << std::endl;
    if (is_page) {
      head << "#include \"basepage.hpp\"" << std::endl;
      // basepage.hpp only FORWARD-declares the request and the response, so a
      // page body's first `request.QUERY' or `response.SET_HEADER' is an
      // "invalid use of incomplete type" unless the real headers come too.
      // The server's request-time compile got them by accident of its include
      // order; an offline compile with the parsi program did not.
      head << "#include \"httprequest.hpp\"" << std::endl;
      head << "#include \"httpresponse.hpp\"" << std::endl;
    }

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "INHERITS" || expr.token.value == "REQUIRES") {
	for (const Expression& ch_expr : expr.args) {
	  std::string inc_name = this->_include_name(ch_expr);
	  requires.push_back(inc_name);
	  head << "#include \"" << inc_name << ".hpp\"" << std::endl;
	}
      }
    }

    // A HPP block goes above the namespace, not inside the class.
    //
    // Not inside the class, because there `struct Net;' would declare a type
    // nested in it, and the CPP block's `struct Net : ...' would be a different
    // type entirely -- the nested one staying incomplete forever, which a
    // member declared shared_ptr<Net> survives right up until the destructor
    // needs it.
    //
    // And above the namespace rather than inside it, so that a block can carry
    // its own #include. That is the whole reason for the two blocks: a model's
    // <torch/torch.h> belongs to the implementation, and anything that lands in
    // this header is inherited by every object that REQUIRES this one.
    // Unqualified names still resolve from inside the namespace, so a member
    // written shared_ptr<Net> finds it.
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "HPP") {
	head << block_text(expr) << std::endl;
      }
    }

    this->_open_namespace(ast.args[0], head);

    if (is_tmpl_class) {
      const Expression* expr = &(ast.args[0]);
      while (!expr->args.empty()) {
	if (expr->token.value == "$tmpl") {
	  head << "template < ";
	  for (const Expression& ch_expr : expr->args)
	    head << "typename " << ch_expr.token.value << ", "; 
	  head.seekp(-2, std::ios::cur);
	  head << " >" << std::endl;
	  break;
	} else if (expr->args.size() > 1) {
	  throw CompileException("domains couldn't be template", ast);
	} else {
	  expr = &(expr->args[0]);
	}
      }
    }

    head << "class " << name;
    if (is_page)
      head << " : public Zigurat::BasePage, ";
    bool is_sub_type = false;
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "INHERITS" || expr.token.value == "BASE") {
	for (const Expression& ch_expr : expr.args) {
	  if (!is_sub_type && !is_page) {
	    is_sub_type = true;
	    head << " : ";
	  }
	  head << "public " << this->_type_name(ch_expr) << ", ";
	}
      }
    }
    if (is_sub_type || is_page) {
      head.seekp(-2, std::ios::cur);
    }
    head << std::endl;
    head << "{" << std::endl;

    if (is_page)
      head << "public: using Zigurat::BasePage::BasePage;" << std::endl;

    for (const Expression& expr : ast.args) {

      if (expr.token.value == "PRIVATE" || expr.token.value == "PUBLIC" || expr.token.value == "PROTECTED") {
	head << Utility::to_lower(expr.token.value) << ':' << std::endl;
      } else if (expr.token.value == "DECLARE") {
	this->_declare(expr, head, 1, true, false, name);
      } else if (expr.token.value == "USING") {
	head << TAB1 << "using " << this->_type_name(expr.args[0]) << ';' << std::endl;
      } else if (expr.token.value == "CONSTRUCTOR") {
	head << TAB1 << name;
	this->_parameters_head(expr.args[0], head, 1);
	head << ';' << std::endl;
      } else if (expr.token.value == "GLOBAL" || expr.token.value == "FUNCTION") {
	
	const Expression& func = (expr.token.value == "GLOBAL") ? expr.args[0] : expr;
	if (func.args[0].args.size() > 0 && func.args[0].args[0].token.value == "$tmpl") { // it's template function
	  head << TAB1 << "template < ";
	  for (const Expression& ch_expr : func.args[0].args[0].args) {
	    head << "typename " << this->_type_name(ch_expr) << ", ";
	  }
	  head.seekp(-2, std::ios::cur);
	  head << " >";
	}
	head << TAB1 << ((expr.token.value == "GLOBAL") ? "static " : "");
	head << this->_type_name(func.args[2].args[0]) << ' ' << func.args[0].token.value;
	if (is_tmpl_class || (func.args[0].args.size() > 0 && func.args[0].args[0].token.value == "$tmpl") ) { // it's template class or template function
	  this->_parameters_tmpl(func.args[1], head, 1);
	  head << std::endl;
	  head << TAB1 << '{' << std::endl;
	  for (const Expression& ch_expr : func.args[3].args) {
	    this->_clause(ch_expr, head, 2);
	  }
	  head << TAB1 << '}' << std::endl;
	} else {
	  this->_parameters_head(func.args[1], head, 1);
	  head << ';' << std::endl;
	}
      
      } else if (expr.token.value == "VIRTUAL") {
	bool is_override = false;
	const Expression* func = &(expr.args[0]);
	if (func->token.value == "OVERRIDE") {
	  func = &(func->args[0]);
	  is_override = true;
	}
	head << TAB1 << "virtual " << this->_type_name(func->args[2].args[0]) << ' ' << func->args[0].token.value;
	this->_parameters_head(func->args[1], head, 1);
	if (is_override)
	  head << " override;" << std::endl;
	else
	  head << ';' << std::endl;
      } else if (expr.token.value == "OVERRIDE") {
	const Expression& func = expr.args[0];
	head << TAB1 << this->_type_name(func.args[2].args[0]) << ' ' << func.args[0].token.value;
	this->_parameters_head(func.args[1], head, 1);
	head << " override;" << std::endl;
      } else if (expr.token.value == "PURE") {
	const Expression& func = expr.args[0].args[0];
	head << TAB1 << "virtual " << this->_type_name(func.args[2].args[0]) << ' ' << func.args[0].token.value;
	this->_parameters_head(func.args[1], head, 1);
	head << " = 0;" << std::endl;
      } else if (expr.token.value == "DESTRUCTOR") {
	head << TAB1 << "virtual ~" << name << "();" << std::endl;
      }

    }

    head << "};" << std::endl;
    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;

    // implementation file

    // THE CPP BLOCK COMES FIRST, BEFORE THIS OBJECT'S OWN HEADER, and the
    // reason is globals.hpp. It defines THIS, CAST, AUTO, VOID, TRUE, FALSE and
    // NULL as bare macros, and a real C++ library is entitled to use any of
    // those as an identifier -- libtorch has
    //
    //     enum class CuDNNDepthwiseKernel { AUTO, CUDNN, NATIVE };
    //
    // in ATen/Context.h, which with AUTO defined reads as `enum class { auto,
    // ... }' and takes the rest of the header down with it. Emitted after the
    // header include, a block could therefore not include libtorch at all.
    //
    // Before it, the block's headers are parsed while those names still mean
    // themselves, and globals.hpp defines the macros afterwards for the
    // generated code that actually wants them. Nothing is lost by the order: a
    // block is independent C++ by construction -- it defines what the HPP block
    // declared -- so it has no reason to see this object's own class first.
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "CPP") {
	impl << block_text(expr) << std::endl;
      }
    }

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;

    this->_open_namespace(ast.args[0], impl);

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "DECLARE") {
	this->_declare(expr, impl, 0, false, true, name);
      } else if (expr.token.value == "CONSTRUCTOR") {
	impl << type_name << "::" << name;
	this->_parameters_impl(expr.args[0], impl, 1);
	if (expr.args.size() > 2) {
	  impl << std::endl;
	  impl << " : ";
	  for (const Expression& ch_expr : expr.args[1].args) {
	    impl << this->_type_name(ch_expr.args[0]) << '(';
	    for (size_t i = 1; i < ch_expr.args.size(); i++) {
	      this->_expr.compile(ch_expr.args[i], impl);
	      impl << ", ";
	    }
	    if (ch_expr.args.size() > 1)
	      impl.seekp(-2, std::ios::cur);
	    impl << "), ";
	  }
	  if (expr.args.size() > 1)
	    impl.seekp(-2, std::ios::cur);
	}
        impl << std::endl;
	impl << '{' << std::endl;
	for (const Expression& ch_expr : expr.args[expr.args.size() - 1].args) {
	  this->_clause(ch_expr, impl, 1);
	}	
	impl << '}' << std::endl;

      } else if (expr.token.value == "FUNCTION" || expr.token.value == "GLOBAL" ||
		 expr.token.value == "VIRTUAL" || expr.token.value == "OVERRIDE") {

	const Expression* func = &expr;
	if (expr.token.value == "FUNCTION") {

	} else if (expr.token.value == "OVERRIDE") {
	  func = &(expr.args[0]);
	} else if (expr.token.value == "VIRTUAL") {
	  func = &(expr.args[0]);
	  if (func->token.value == "FUNCTION") {

	  } else if (func->token.value == "OVERRIDE") {
	    func = &(func->args[0]);
	  } else {
	    throw CompileException("unknown keyword", *func);
	  }
	} else if (expr.token.value == "GLOBAL") {
	  func = &(expr.args[0]);
	} else {
	  throw CompileException("unknown keyword", *func);
	}

	if (func->args[0].args.size() == 0 && !is_tmpl_class) { // it's not template
	  impl << this->_type_name(func->args[2].args[0]) << ' ' << type_name << "::" << func->args[0].token.value;
	  this->_parameters_impl(func->args[1], impl, 1);
	  impl << std::endl;
	  impl << '{' << std::endl;
	  for (const Expression& ch_expr : func->args[3].args) {
	    this->_clause(ch_expr, impl, 1);
	  }
	  impl << '}' << std::endl;
	}

      } else if (expr.token.value == "DESTRUCTOR") {
	impl << type_name << "::~" << name << "()" << std::endl;
	impl << '{' << std::endl;
	for (const Expression& ch_expr : expr.args[0].args) {
	  this->_clause(ch_expr, impl, 1);
	}
	impl << '}' << std::endl;
      }
    }

    this->_close_namespace(ast.args[0], impl);

    if (is_page) {
      impl << "extern \"C\" " << type_name << "* new_page";
      impl << "(Zigurat::binarystream* client, Zigurat::HTTPRequest& request, Zigurat::HTTPResponse& response)" << std::endl;
      impl << '{' << std::endl;
      impl << TAB1 << "return new " << type_name << "(*client, request, response);" << std::endl;
      impl << '}' << std::endl;
      impl << "extern \"C\" void delete_page(" << type_name << "* page)" << std::endl;
      impl << '{' << std::endl;
      impl << TAB1 << "delete page;" << std::endl;
      impl << '}' << std::endl;
    }

    // config file

    if (is_page)
      conf << "PAGE:" << std::endl;
    else
      conf << "CLASS:" << std::endl;
    conf << TAB1 << "NAME: " << name << std::endl;
    conf << TAB1 << "DOMAIN: " << this->_domain(ast.args[0]) << std::endl;
    conf << TAB1 << "FULL_NAME: " << full_name << std::endl;
    conf << TAB1 << "TYPE_NAME: " << type_name << std::endl;
    path_conf(conf, TAB1, TAB2, type_name);
    conf << TAB1 << "GUARD_NAME: " << guard_name << std::endl;
    conf << TAB1 << "HASH_KEY: " << hash_key << std::endl;
    conf << TAB1 << "INHERITS:" << std::endl;
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "INHERITS") {
	for (const Expression& ch_expr : expr.args) {
	  conf << TAB2 << "INHERIT: " << this->_type_name(ch_expr) << std::endl;
	}    
      }
    }
    conf << TAB1 << "REQUIRES:" << std::endl;
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "REQUIRES") {
	for (const Expression& ch_expr : expr.args) {
	  conf << TAB2 << "REQUIRE: " << this->_type_name(ch_expr) << std::endl;
	} 
      }
    }
        
    this->_build(include_name, requires, head, impl, conf, ast, type_name, false);
  }

  void Compiler::_type(const Expression& ast)
  {
    std::string name = this->_name(ast.args[0]);
    std::string full_name = this->_full_name(ast.args[0]);
    std::string type_name = this->_type_name(ast.args[0]);
    std::string guard_name = this->_guard_name(ast.args[0]);
    std::string include_name = this->_include_name(ast.args[0]);
    std::string hash_key = SHA::checksum(SHA::SHA1, type_name);
    std::stringstream head;
    std::stringstream impl;
    std::stringstream conf;
    std::list<std::string> requires;
    bool is_tmpl_class = type_name.back() == '>';
  
    std::string inc_name = this->_include_name(ast.args[ast.args.size() - 1]);
    requires.push_back(inc_name);
    head << "#include \"" << inc_name << ".hpp\"" << std::endl;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "REQUIRES") {
	for (const Expression& ch_expr : expr.args) {
	  std::string inc_name = this->_include_name(ch_expr);
	  requires.push_back(inc_name);
	  head << "#include \"" << inc_name << ".hpp\"" << std::endl;
	}
      }
    }

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;
    this->_open_namespace(ast.args[0], head);

    if (is_tmpl_class) {
      const Expression* expr = &(ast.args[0]);
      while (!expr->args.empty()) {
	if (expr->token.value == "$tmpl") {
	  head << "template < ";
	  for (const Expression& ch_expr : expr->args)
	    head << "typename " << ch_expr.token.value << ", "; 
	  head.seekp(-2, std::ios::cur);
	  head << " >" << std::endl;
	  break;
	} else if (expr->args.size() > 1) {
	  throw CompileException("domains couldn't be template", ast);
	} else {
	  expr = &(expr->args[0]);
	}
      }
    }

    head << "using " << name << " = " << this->_type_name(ast.args[ast.args.size() - 1]) << ';' << std::endl;

    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;
 
    // implementation file

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;
    this->_open_namespace(ast.args[0], impl);
    this->_close_namespace(ast.args[0], impl);

    // config file

    conf << "TYPE:" << std::endl;
    conf << TAB1 << "NAME: " << name << std::endl;
    conf << TAB1 << "DOMAIN: " << this->_domain(ast.args[0]) << std::endl;
    conf << TAB1 << "FULL_NAME: " << full_name << std::endl;
    conf << TAB1 << "TYPE_NAME: " << type_name << std::endl;
    path_conf(conf, TAB1, TAB2, type_name);
    conf << TAB1 << "GUARD_NAME: " << guard_name << std::endl;
    conf << TAB1 << "HASH_KEY: " << hash_key << std::endl;
    conf << TAB1 << "AS:" << this->_type_name(ast.args[1]) << std::endl;
   
    this->_build(include_name, requires, head, impl, conf, ast, type_name, false);
  }

  void Compiler::_enum(const Expression& ast)
  {
    std::string name = this->_name(ast.args[0]);
    std::string full_name = this->_full_name(ast.args[0]);
    std::string type_name = this->_type_name(ast.args[0]);
    std::string guard_name = this->_guard_name(ast.args[0]);
    std::string include_name = this->_include_name(ast.args[0]);
    std::string hash_key = SHA::checksum(SHA::SHA1, type_name);
    std::stringstream head;
    std::stringstream impl;
    std::stringstream conf;
    std::list<std::string> requires;
    
    // header file

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;
    head << "#include <cstdint>" << std::endl;
    head << "#include <string>" << std::endl;
    head << "#include <vector>" << std::endl;
    this->_open_namespace(ast.args[0], head);
    head << "enum class " << name << " : uint8_t" << std::endl;
    head << "{" << std::endl;
    for (const Expression& expr : ast.args[1].args) {
      head << TAB1 << expr.token.value << ',' << std::endl;
    }
    head.seekp(-1 - ENDL_LENGTH, std::ios::cur);
    head << std::endl << "};" << std::endl;
    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;
 
    // implementation file

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;
    this->_open_namespace(ast.args[0], impl);
    this->_close_namespace(ast.args[0], impl);

    // config file

    conf << "ENUM:" << std::endl;
    conf << TAB1 << "NAME: " << name << std::endl;
    conf << TAB1 << "DOMAIN: " << this->_domain(ast.args[0]) << std::endl;
    conf << TAB1 << "FULL_NAME: " << full_name << std::endl;
    conf << TAB1 << "TYPE_NAME: " << type_name << std::endl;
    path_conf(conf, TAB1, TAB2, type_name);
    conf << TAB1 << "GUARD_NAME: " << guard_name << std::endl;
    conf << TAB1 << "HASH_KEY: " << hash_key << std::endl;
    conf << TAB1 << "ELEMENTS:" << this->_type_name(ast.args[1]) << std::endl;
    for (const Expression& expr : ast.args[1].args) {
      conf << TAB2 << "ELEMENT: " << expr.token.value << std::endl;
    }

    this->_build(include_name, requires, head, impl, conf, ast, type_name, false);
  }

  void Compiler::_sequence(const Expression& ast)
  {
    std::string name = this->_name(ast.args[0]);
    std::string full_name = this->_full_name(ast.args[0]);
    std::string type_name = this->_type_name(ast.args[0]);
    std::string guard_name = this->_guard_name(ast.args[0]);
    std::string include_name = this->_include_name(ast.args[0]);
    std::string hash_key = SHA::checksum(SHA::SHA1, type_name);
    std::stringstream head;
    std::stringstream impl;
    std::stringstream conf;
    std::list<std::string> requires;
  
    // header file

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;
    head << "#include \"engine-compat.hpp\"" << std::endl;
    size_t offset = 0;
    for (const Expression& expr : ast.args) {
      if (expr.token.value == "REQUIRES") {
	for (const Expression& ch_expr : expr.args) {
	  std::string inc_name = this->_include_name(ch_expr);
	  requires.push_back(inc_name);
	  head << "#include \"" << inc_name << ".hpp\"" << std::endl;
	}
	offset = 1;
	break;
      }
    }
    
    this->_open_namespace(ast.args[0], head);
    head << "class " << name << std::endl;
    head << "{" << std::endl;
    head << "private:" << std::endl;
    head << TAB1 << "static Sequence _seq;" << std::endl;
    head << TAB1 << "static void _attach();" << std::endl;
    head << "public:" << std::endl;
    head << TAB1 << "static Zigurat::hashkey_t hash_key;" << std::endl;
    head << TAB1 << "static const Zigurat::String NAME;" << std::endl;
    head << TAB1 << "static const std::vector<std::string> PATH;" << std::endl;
    head << TAB1 << "static const Zigurat::Long FROM;" << std::endl;
    head << TAB1 << "static const Zigurat::Long TO;" << std::endl;
    head << TAB1 << "static const Zigurat::Long STEP;" << std::endl;
    head << TAB1 << "static Zigurat::Long CURRENT();" << std::endl;
    head << TAB1 << "static void SET_CURRENT(Zigurat::Long);" << std::endl;
    head << TAB1 << "static Zigurat::Long NEXT();" << std::endl;
    head << TAB1 << "static Zigurat::Long BACK();" << std::endl;
    head << TAB1 << "static void RESET();" << std::endl;
    head << "};" << std::endl;
    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;
 
    // implementation file

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;
    this->_open_namespace(ast.args[0], impl);

    impl << TAB1 << "Zigurat::hashkey_t " << type_name << "::hash_key = {";
    for (size_t i = 0; i < hash_key.size(); i+=2)
      impl << "0x" << hash_key[i] << hash_key[i + 1] << ',';
    impl.seekp(-1, std::ios::cur);
    impl << TAB1 << "};" << std::endl;
    impl << TAB1 << "const Zigurat::String " << type_name << "::NAME = \"" << type_name << "\";" << std::endl;
    impl << TAB1 << "const std::vector<std::string> " << type_name << "::PATH = {";
    {
      const std::vector<std::string> path = name_path(type_name);
      for (size_t i = 0; i < path.size(); i++)
	impl << ((i > 0) ? ", " : "") << "\"" << path[i] << "\"";
    }
    impl << "};" << std::endl;
    impl << TAB1 << "const Zigurat::Long " << type_name << "::FROM = ";
    this->_expr.compile(ast.args[offset + 1].args[0], impl);
    impl << ';' << std::endl;
    impl << TAB1 << "const Zigurat::Long " << type_name << "::TO = ";
    this->_expr.compile(ast.args[offset + 2].args[0], impl);
    impl << ';' << std::endl;
    impl << TAB1 << "const Zigurat::Long " << type_name << "::STEP = ";
    this->_expr.compile(ast.args[offset + 3].args[0], impl);
    impl << ';' << std::endl;
    impl << TAB1 << "Sequence " << type_name << "::_seq;" << std::endl;
    // attach lazily and once: parsic loads objects with no store open,
    // so nothing here may run at static initialization
    impl << TAB1 << "void " << type_name << "::_attach()" << std::endl;
    impl << TAB1 << "{" << std::endl;
    impl << TAB2 << "static bool engine_attached = ([] () -> bool {" << std::endl;
    impl << TAB3 << "_seq.m = ::globals_memory();" << std::endl;
    impl << TAB3 << "_seq.name = \"" << type_name << "\";" << std::endl;
    impl << TAB3 << "_seq.hash_key = intern_key(" << type_name << "::hash_key);" << std::endl;
    impl << TAB3 << "_seq.from = " << type_name << "::FROM.value();" << std::endl;
    impl << TAB3 << "_seq.to = " << type_name << "::TO.value();" << std::endl;
    impl << TAB3 << "_seq.step = " << type_name << "::STEP.value();" << std::endl;
    impl << TAB3 << "_seq.initialized = 0;" << std::endl;
    impl << TAB3 << "pthread_mutex_init(&_seq.guard, nullptr);" << std::endl;
    impl << TAB3 << "return true;" << std::endl;
    impl << TAB2 << "})();" << std::endl;
    impl << TAB2 << "(void)engine_attached;" << std::endl;
    impl << TAB1 << "}" << std::endl;
    impl << TAB1 << "Zigurat::Long " << type_name << "::CURRENT() { _attach(); return Zigurat::Long(seq_current(&_seq)); }" << std::endl;
    impl << TAB1 << "void " << type_name << "::SET_CURRENT(Zigurat::Long v) { _attach(); seq_set_current(&_seq, v.value()); }" << std::endl;
    impl << TAB1 << "Zigurat::Long " << type_name << "::NEXT() { _attach(); return Zigurat::Long(seq_next(&_seq)); }" << std::endl;
    impl << TAB1 << "Zigurat::Long " << type_name << "::BACK() { _attach(); return Zigurat::Long(seq_back(&_seq)); }" << std::endl;
    impl << TAB1 << "void " << type_name << "::RESET() { _attach(); seq_reset(&_seq); }" << std::endl;

    this->_close_namespace(ast.args[0], impl);

    // config file

    conf << "SEQUENCE:" << std::endl;
    conf << TAB1 << "NAME: " << name << std::endl;
    conf << TAB1 << "DOMAIN: " << this->_domain(ast.args[0]) << std::endl;
    conf << TAB1 << "FULL_NAME: " << full_name << std::endl;
    conf << TAB1 << "TYPE_NAME: " << type_name << std::endl;
    path_conf(conf, TAB1, TAB2, type_name);
    conf << TAB1 << "GUARD_NAME: " << guard_name << std::endl;
    conf << TAB1 << "HASH_KEY: " << hash_key << std::endl;
    conf << TAB1 << "FROM:"; 
    this->_expr.compile(ast.args[offset + 1].args[0], conf);
    conf << std::endl;
    conf << TAB1 << "TO:";
    this->_expr.compile(ast.args[offset + 2].args[0], conf);
    conf << std::endl;
    conf << TAB1 << "STEP:";
    this->_expr.compile(ast.args[offset + 3].args[0], conf);
    conf << std::endl;
    
    // the same sequence again, as one .cicili for the Cicili MVCCS
    {
      std::stringstream cicili;
      cicili << ";;;; " << type_name << " -- generated by the Parsi compiler beside the" << std::endl;
      cicili << ";;;; C++ pair. One file is header and source both: import it into a" << std::endl;
      cicili << ";;;; target that imports the Cicili MVCCS engine (MVCCS-cicili/), whose" << std::endl;
      cicili << ";;;; defsequence macro expands the object. Attach once, after" << std::endl;
      cicili << ";;;; memory_open." << std::endl;
      cicili << std::endl;

      // the bound constants become literals a Lisp reader can take
      auto cicili_expr([&] (const Expression& expr) -> std::string {
	  std::stringstream text;
	  this->_expr.compile(expr, text);
	  std::string value = text.str();
	  if (value == "LONG::MAX") return "9223372036854775807";
	  if (value == "LONG::MIN") return "-9223372036854775808";
	  return value;
	});

      cicili << "(DEFMACRO define-" << full_name << " ()" << std::endl;
      cicili << "  '($$$" << std::endl;
      cicili << "    (defsequence " << type_name << " ";
      cicili << cicili_expr(ast.args[offset + 1].args[0]) << " ";
      cicili << cicili_expr(ast.args[offset + 2].args[0]) << " ";
      cicili << cicili_expr(ast.args[offset + 3].args[0]) << ")))" << std::endl;

      this->_cicili_file(include_name, cicili.str());
    }

    this->_build(include_name, requires, head, impl, conf, ast, type_name, true);
  }

}
