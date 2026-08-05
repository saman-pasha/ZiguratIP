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

  void Compiler::_include(const Expression& ast)
  {
    this->_includes.push_back(ast.args[0].token.value.substr(1, ast.args[0].token.value.size() - 2));
  }

  void Compiler::_link(const Expression& ast)
  {
    this->_links.push_back(ast.args[0].token.value.substr(1, ast.args[0].token.value.size() - 2));
  }

  std::vector<Compiler::index_desc_t> Compiler::_table_indexes(const Expression& ast)
  {
    std::string full_name = this->_full_name(ast.args[0]);
    std::vector<index_desc_t> indexes;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	if (expr.args.size() > 2) {
	  if (expr.args[2].token.value == "PRIMARY") {
	    indexes.emplace_back(std::vector<std::string>{expr.args[0].token.value}, std::vector<std::string>{expr.args[1].token.value}, 
				 expr.args[2].token.value, "IDX_" + full_name + "_" + expr.args[0].token.value, true);
	  } else if (expr.args[2].token.value == "UNIQUE") {
	    indexes.emplace_back(std::vector<std::string>{expr.args[0].token.value}, std::vector<std::string>{expr.args[1].token.value}, 
				 expr.args[2].token.value, "IDX_" + full_name + "_" + expr.args[0].token.value, true);
	  } else if (expr.args[2].token.value == "INDEX") {
	    indexes.emplace_back(std::vector<std::string>{expr.args[0].token.value}, std::vector<std::string>{expr.args[1].token.value}, 
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
	      types.push_back(expr.args[1].token.value);
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

    head << "#include \"globals.hpp\"" << std::endl;
    head << "#include \"basetable.hpp\"" << std::endl;
    head << "#include \"btreeindex.hpp\"" << std::endl;
    
    if (ast.args.size() > 1 && ast.args[2].token.value == "REQUIRES") {
      for (const Expression& expr : ast.args[1].args) {
	std::string inc_name = this->_include_name(expr);
	requires.push_back(inc_name);
	head << "#include \"" << inc_name << ".hpp\"" << std::endl;
      }
    }

    this->_open_namespace(ast.args[0], head);
    head << "class " << name << " : public Zigurat::BaseTable" << std::endl;
    head << "{" << std::endl;
    head << "private:" << std::endl;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "COLUMN") {
	head << TAB1 << this->_type_name(expr.args[1]) << " _" << expr.args[0].token.value;
	head <<';' << std::endl;
      }
    }

    head << "public:" << std::endl;
    head << TAB1 << "using Zigurat::BaseTable::BaseTable;" << std::endl;
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

    head << TAB1 << "int64_t pack_size() override;" << std::endl;
    head << TAB1 << "void prepare() override;" << std::endl;
    head << TAB1 << "void map() override;" << std::endl;
    head << TAB1 << "void unmap() override;" << std::endl;
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
      impl << "> " << name << "::" << std::get<3>(index) << std::endl;

      impl << TAB1 << "(Globals::memory(), \"" << std::get<3>(index) << "\", "
	   << ((std::get<4>(index)) ? "true, " : "false, ");
      for (const std::string& column : std::get<0>(index))
        impl << name << "::" << column << ", ";
      impl.seekp(-2, std::ios::cur);
      impl << ");" << std::endl;
    }

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

    impl << "void " << name << "::map()" << std::endl; // BTreeIndex map
    impl << '{' << std::endl;
    for (const index_desc_t& index : indexes) {
      impl << TAB1 << name << "::" << std::get<3>(index) << ".map(*this);" << std::endl;
    }
    impl << '}' << std::endl;

    impl << "void " << name << "::unmap()" << std::endl; // BTreeIndex unmap
    impl << '{' << std::endl;
    for (const index_desc_t& index : indexes) {
      impl << TAB1 << name << "::" << std::get<3>(index) << ".unmap(*this);" << std::endl;
    }
    impl << '}' << std::endl;

    impl << "void " << name << "::truncate_indexes()" << std::endl; // BTreeIndex truncate
    impl << '{' << std::endl;
    for (const index_desc_t& index : indexes) {
      impl << TAB1 << name << "::" << std::get<3>(index) << ".truncate();" << std::endl;
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
      
    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;
    head << "#include \"globals.hpp\"" << std::endl;
    
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

    this->_open_namespace(ast.args[0], head);

    head << TAB1 << this->_type_name(ast.args[2].args[0]);
    head << ' ' << name;
    this->_parameters_head(ast.args[1], head, 1);
    head << ";" << std::endl;

    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;

    head << "extern \"C\" void call();" << std::endl;

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
      
    // header file

    head << "#ifndef " << guard_name << std::endl;
    head << "#define " << guard_name << std::endl;

    head << "#include \"globals.hpp\"" << std::endl;
    if (is_page)
      head << "#include \"basepage.hpp\"" << std::endl;

    for (const Expression& expr : ast.args) {
      if (expr.token.value == "INHERITS" || expr.token.value == "REQUIRES") {
	for (const Expression& ch_expr : expr.args) {
	  std::string inc_name = this->_include_name(ch_expr);
	  requires.push_back(inc_name);
	  head << "#include \"" << inc_name << ".hpp\"" << std::endl;
	}
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
    head << "#include \"basesequence.hpp\"" << std::endl;
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
    head << "class " << name << " : public Zigurat::BaseSequence<" << name << ">" << std::endl;
    head << "{" << std::endl;
    head << "public:" << std::endl;
    head << TAB1 << "static const Zigurat::hashkey_t hash_key;" << std::endl;
    head << TAB1 << "static const Zigurat::String NAME;" << std::endl;
    head << TAB1 << "static const std::vector<std::string> PATH;" << std::endl;
    head << TAB1 << "static const Zigurat::Long FROM;" << std::endl;
    head << TAB1 << "static const Zigurat::Long TO;" << std::endl;
    head << TAB1 << "static const Zigurat::Long STEP;" << std::endl;
    head << TAB1 << name << "();" << std::endl;
    head << TAB1 << name << "(Zigurat::Pointer&&);" << std::endl;
    head << TAB1 << name << "(const Zigurat::Pointer&);" << std::endl;
    head << "};" << std::endl;
    this->_close_namespace(ast.args[0], head);
    head << "#endif // " << guard_name << std::endl;
 
    // implementation file

    impl << "#include \"" << include_name << ".hpp\"" << std::endl;
    this->_open_namespace(ast.args[0], impl);

    impl << TAB1 << "const Zigurat::hashkey_t " << type_name << "::hash_key = {";
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
    impl << TAB1 << name << "::" << name << "()" << std::endl;
    impl << TAB1 << "{" << std::endl;
    impl << TAB1 << "}" << std::endl;
    impl << TAB1 << name << "::" << name << "(Zigurat::Pointer&& pointer)" << std::endl;
    impl << TAB1 << "{" << std::endl;
    impl << TAB2 << "this->pointer = std::move(pointer);" << std::endl;
    impl << TAB1 << "}" << std::endl;
    impl << TAB1 << name << "::" << name << "(const Zigurat::Pointer& pointer)" << std::endl;
    impl << TAB1 << "{" << std::endl;
    impl << TAB2 << "this->pointer = pointer;" << std::endl;
    impl << TAB1 << "}" << std::endl;

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
    
    this->_build(include_name, requires, head, impl, conf, ast, type_name, true);
  }

}
