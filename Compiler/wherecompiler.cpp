#include "wherecompiler.hpp"
#include "expression.hpp"
#include "compiler.hpp"
#include <iostream>


namespace Zigurat
{

  WhereCompiler::WhereCompiler(const Compiler& compiler, const Expression& from)
    : _compiler(compiler), 
      _from(from), 
      _name((from.args.size() > 1) ? compiler._name(from.args[1]) : compiler._name(from.args[0])),
      _type_name(compiler._type_name(from.args[0])),
      _catalog(compiler._catalog_path + compiler._include_name(from.args[0]) + ".conf"),
      _expr(compiler, {from})
  {

  }

  void WhereCompiler::compile(const Expression* where, std::stringstream& code, int lvl, std::function<void (int)> content) const
  {
    if (where) {
      if (where->args[0].token.value == "AND") {
	this->_compile(*where, code, lvl, content, &where->args[0]);
      } else if (where->args[0].token.value == "OR") {
	// OR reads every row and filters, like BETWEEN and LIKE. Opening one
	// cursor per operand instead would emit a row twice when both sides
	// match it, and there is nowhere to deduplicate. (This branch used to
	// call compile() with its own arguments, which recursed until the stack
	// ran out -- an OR in a WHERE clause killed the compiler outright.)
	this->_compile(*where, code, lvl, content, nullptr);
      } else {
	this->_compile(*where, code, lvl, content, nullptr);
      }
    } else { // Query without where
      std::string tab(lvl, '\t');
      code << tab << "Globals::memory()->cursor<" << this->_type_name << ">" << std::endl;
      code << tab << "([&] (" << this->_type_name << "& " << this->_name << ") -> bool {" << std::endl;
      content(lvl + 1);
      code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
      code << tab << "});" << std::endl;
    }
  }

  void WhereCompiler::_compile(const Expression& where, std::stringstream& code, int lvl, std::function<void (int)>& content,
			       const Expression* bitwise) const
  {
    const Expression& opr = (bitwise) ? bitwise->args[0] : where.args[0];

    // BETWEEN carries three operands, so it does not fit the single-bound cursor
    // calls below. It falls through to the scan, where the expression compiler
    // expands it to (x >= low) && (x <= high) and applies that as a filter.
    // Writing the two comparisons out by hand instead lets the leading one pick
    // up the index.
    if (opr.token.value != "BETWEEN" && opr.token.value != "LIKE" &&
	opr.args[0].token.value == "$obj" && (opr.args[0].args[0].token.value[0] != '@' || 
					      (opr.args[0].args[0].token.value == "." && opr.args[0].args[0].args[0].token.value[0] != '@'))) {
      const Expression& obj = opr.args[0];
      const std::string clm = (obj.args[0].token.value != ".") ? obj.args[0].token.value : obj.args[0].args[1].token.value;

      std::list<const Option*> opts;
      if (this->_catalog.extract("/TABLE/KEYS/$KEY/COLUMNS/COLUMN:" + clm, opts)) {
	
	const Option* opt_key = *opts.begin();
	std::string index_name;
	this->_catalog.get(*opt_key, "/NAME", index_name);

	std::list<std::string> columns;
	if (this->_catalog.list(*opt_key, "/COLUMNS/COLUMN", columns)) {

	  if (*columns.begin() == clm) {

	    std::list<std::string> types;
	    this->_catalog.list(*opt_key, "/TYPES/TYPE", types);

	    // A STRING KEY RIDES AS A HASH in the Cicili engine, so its
	    // tree orders hashes, not text: equality survives (and every
	    // indexed lookup re-applies its full predicate to each row the
	    // index hands back, so a collision costs a visit and never a
	    // wrong answer), but a range over it would answer garbage.
	    // Anything but equality on a string-keyed level scans instead.
	    const std::string cursor_kind = this->_cursor_name(opr.token.value);
	    const std::string& lead_type = *types.begin();
	    const bool string_keyed = (lead_type.find("STRING") != std::string::npos ||
				       lead_type.find("TEXT") != std::string::npos);

	    std::string tab(lvl, '\t');
    	    if (string_keyed && cursor_kind != "cursor_equal") {
	      // fall through to the scan below
	    } else if (columns.size() == 1) { // Single Level Index
	      code << tab << this->_type_name << "::" << index_name << '.' << this->_cursor_name(opr.token.value) << std::endl;
	      code << tab << "(";
	      this->_expr.compile(opr.args[1], code);
	      code << ", [&] (" << this->_type_name << "& " << this->_name << ") -> bool {" << std::endl;
	      code << tab << this->_compiler.TAB1 << "if (";
	      this->_expr.compile(where.args[0], code);
	      code << ") {" << std::endl;	
	      content(lvl + 2);
	      code << tab << this->_compiler.TAB1 << "}" << std::endl;
	      code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	      code << tab << "});" << std::endl;
	    } else { // Multi Level Index
	      this->_cursor(where, code, lvl, content, bitwise, opr, index_name, columns, columns.begin(), types, types.begin());
	    }

	    return;
	  }
	}
      }
    }

    // When can not recognise index
    std::string tab(lvl, '\t');
    code << tab << "Globals::memory()->cursor<" << this->_type_name << ">" << std::endl;
    code << tab << "([&] (" << this->_type_name << "& " << this->_name << ") -> bool {" << std::endl;
    code << tab << this->_compiler.TAB1 << "if (";
    this->_expr.compile(where.args[0], code);
    code << ") {" << std::endl;	
    content(lvl + 2);
    code << tab << this->_compiler.TAB1 << "}" << std::endl;
    code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
    code << tab << "});" << std::endl;
  }

  void WhereCompiler::_cursor(const Expression& where, std::stringstream& code, int lvl, std::function <void (int)>& content,
			      const Expression* bitwise, const Expression& opr, const std::string& index_name,
			      const std::list<std::string>& columns, typename std::list<std::string>::iterator columns_iter, 
			      const std::list<std::string>& types, typename std::list<std::string>::iterator types_iter) const
  {
    std::string tab(lvl, '\t');
    
    std::function<void (int)> next([&] (int lvl) {
	
	std::string tab(lvl, '\t');
	if (bitwise) {
	  if (bitwise->args[1].token.type == TokenType::OP && bitwise->args[1].token.value == "AND") {
	    this->_cursor(where, code, lvl + 1, content, &bitwise->args[1], bitwise->args[1].args[0], 
			  index_name, columns, ++columns_iter, types, ++types_iter);
	  } else if (bitwise->args[1].token.type == TokenType::OP && bitwise->args[1].token.value == "OR") {
	    this->_cursor(where, code, lvl + 1, content, &bitwise->args[1], bitwise->args[1].args[0], 
			  index_name, columns, ++columns_iter, types, ++types_iter);
	    this->_cursor(where, code, lvl + 1, content, &bitwise->args[1], bitwise->args[1].args[1], 
			  index_name, columns, columns_iter, types, types_iter);
	  } else {
	    this->_cursor(where, code, lvl + 1, content, nullptr, bitwise->args[1], 
			  index_name, columns, ++columns_iter, types, ++types_iter);
	  }
	} else { // Partial Search
	  // NO KEY COLUMN LEFT: the levels above bound every column of the
	  // key, so `_btreeindex_' here is the innermost handle and its
	  // cursor hands out rows. `kb == K AND flag == 1' over a (kb, name)
	  // key lands here when `flag' is not a key column at all. Before
	  // this branch existed the loop below ran ZERO times and emitted
	  // NOTHING -- an empty lambda, a cursor that opened and closed, no
	  // rows, no error; predicates.zt's D lines were its first victim.
	  if (std::next(columns_iter) == columns.end()) {
	    code << tab << "_btreeindex_.cursor" << std::endl;
	    code << tab << "([&] (" << this->_type_name << "& " << this->_name << ") -> bool {" << std::endl;
	    code << tab << this->_compiler.TAB1 << "if (";
	    this->_expr.compile(where.args[0], code);
	    code << ") {" << std::endl;
	    content(lvl + 2);
	    code << tab << this->_compiler.TAB1 << "}" << std::endl;
	    code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	    code << tab << "});" << std::endl;
	  }
	  while (++columns_iter != columns.end()) {
	    ++types_iter;

	    if (columns_iter == --columns.end()) { // Inner

	      code << tab << "_btreeindex_.cursor" << std::endl;
	      code << tab << "([&] (" << this->_type_name << "& " << this->_name << ") -> bool {" << std::endl;
	      code << tab << this->_compiler.TAB1 << "if (";
	      this->_expr.compile(where.args[0], code);
	      code << ") {" << std::endl;	
	      content(lvl + 2);
	      code << tab << this->_compiler.TAB1 << "}" << std::endl;
	      code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	      code << tab << "});" << std::endl;

	    } else { // Middle

	      code << tab << "_btreeindex_.cursor" << std::endl;
	      code << tab << "([&] (Zigurat::BTreeIndex<" << this->_type_name << ", ";
	      std::list<std::string>::iterator iter = types_iter;
	      while (++iter != types.end()) {
		code << *iter << ", ";
	      }
	      code.seekp(-2, std::ios::cur);
	      code << ">& _btreeindex_) -> bool {" << std::endl;
	      code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	      code << tab << "});" << std::endl;

	    }

	  }
	}

      });

    // When searched column exists in columns order. On the Cicili
    // engine, a NON-innermost level serves full and equal walks only
    // (bt_cursor_dep / bt_cursor_equal_dep); a range there -- and any
    // non-equality over a hashed string level -- takes the partial-
    // search walk instead, which descends everything and filters with
    // the full predicate: slower, never wrong.
    const std::string level_cursor = this->_cursor_name(opr.token.value);
    const bool level_string = ((*types_iter).find("STRING") != std::string::npos ||
			       (*types_iter).find("TEXT") != std::string::npos);
    const bool level_inner = (std::next(columns_iter) == columns.end());
    const bool level_ok = (level_cursor == "cursor_equal")
      || (level_inner && !level_string && level_cursor != "cursor");
    if (level_ok &&
	opr.args[0].token.value == "$obj" && (opr.args[0].args[0].token.value == *columns_iter || 
					      (opr.args[0].args[0].token.value == "." && opr.args[0].args[0].args[0].token.value == *columns_iter))) {

      if (columns_iter == columns.begin()) { // Outer

	code << tab << this->_type_name << "::" << index_name << '.' << this->_cursor_name(opr.token.value) << std::endl;
	code << tab << "(";
	this->_expr.compile(opr.args[1], code);
	code << ", [&] (Zigurat::BTreeIndex<" << this->_type_name << ", ";
	std::list<std::string>::iterator iter = types_iter;
	while (++iter != types.end()) {
	  code << *iter << ", ";
	}
	code.seekp(-2, std::ios::cur);
	code << ">& _btreeindex_) -> bool {" << std::endl;
	next(lvl + 1);
	code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	code << tab << "});" << std::endl;

      } else if (columns_iter == --columns.end()) { // Inner

	code << tab << "_btreeindex_." << this->_cursor_name(opr.token.value) << std::endl;
	code << tab << "(";
	this->_expr.compile(opr.args[1], code);
	code << ", [&] (" << this->_type_name << "& " << this->_name << ") -> bool {" << std::endl;
	code << tab << this->_compiler.TAB1 << "if (";
	this->_expr.compile(where.args[0], code);
	code << ") {" << std::endl;	
	content(lvl + 2);
	code << tab << this->_compiler.TAB1 << "}" << std::endl;
	code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	code << tab << "});" << std::endl;

      } else { // Middle

	code << tab << "_btreeindex_." << this->_cursor_name(opr.token.value) << std::endl;
	code << tab << "(";
	this->_expr.compile(opr.args[1], code);
	code << ", [&] (Zigurat::BTreeIndex<" << this->_type_name << ", ";
	std::list<std::string>::iterator iter = types_iter;
	while (++iter != types.end()) {
	  code << *iter << ", ";
	}
	code.seekp(-2, std::ios::cur);
	code << ">& _btreeindex_) -> bool {" << std::endl;
	next(lvl + 1);
	code << tab << this->_compiler.TAB1 << "return true;" << std::endl;
	code << tab << "});" << std::endl;
      }

    } else { // When searched column does not exists in columns order

      next(lvl);

    }
  }

  std::string WhereCompiler::_cursor_name(const std::string& opr) const
  {
    if (opr == "=" || opr == "==" || opr == "IS")
      return "cursor_equal";
    else if (opr == "<>")
      return "cursor_not_equal";
    else if (opr == "<")
      return "cursor_less_than";
    else if (opr == "<=")
      return "cursor_less_than_equal";
    else if (opr == ">")
      return "cursor_greater_than";
    else if (opr == ">=")
      return "cursor_greater_than_equal";
    return "cursor";
  }

}
