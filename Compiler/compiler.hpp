
#ifndef __COMPILER_HPP__
#define __COMPILER_HPP__

#include <string>
#include <list>
#include <utility>
#include <vector>
#include <tuple>
#include <functional>
#include <iostream>
#include "exprcompiler.hpp"

namespace Zigurat 
{

  class Option;
  class Configuration;
  class Expression;
  class Parser;
  class HeadCompiler;
  class WhereCompiler;

  class Compiler
  {
  private:
    static std::string TAB1;
    static std::string TAB2;
    static std::string TAB3;
    static std::string TAB4;
    static std::string TAB5;
    std::string ENDL;
    int ENDL_LENGTH;

  public:
    typedef std::vector<std::string> (*links_t) (void);
    //                  columns                   columns types             index type   index name  unique
    typedef std::tuple< std::vector<std::string>, std::vector<std::string>, std::string, std::string, bool> index_desc_t;
    Compiler();
    Compiler(std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, bool);
    void configure(std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, std::string, bool);
    void compile(const Expression&);
    virtual ~Compiler();
  
  private:
    const ExprCompiler _expr;

    std::string _cpp;
    std::string _cpp_flags;
    std::string _ld_flags;
    std::string _catalog_path;
    std::string _include_path;
    std::string _obj_path;
    std::string _lib_path;
    std::string _tmp_path;
    std::string _ld_path;
    bool        _trace_mode;
    
    std::list<std::string> _includes;
    std::list<std::string> _links;
    // Everything a generated object can reach: the row types alone pull in
    // Type, and the storage engine reaches Encoding, Compression and Threading.
    // A missing entry here surfaces as undefined symbols at link time in the
    // middle of compiling a user's table.
    std::vector<std::string> _libs = {"Core", "StreamIO", "Type", "Library",
				      "Compression", "Encoding", "Cryptography",
				      "Configuration", "Threading", "SocketIO",
				      "MVCCS", "HTTP", "pthread"};

    void _build(std::string, std::list<std::string>&, 
		std::stringstream&, std::stringstream&, std::stringstream&, const Expression&);

    void _clause(const Expression&, std::stringstream&, int);
    void _suite(const Expression&);

    std::string _name(const Expression&) const;
    std::string _domain(const Expression&) const;
    std::string _full_name(const Expression&) const;
    std::string _type_name(const Expression&) const;
    std::string _guard_name(const Expression&) const;
    std::string _include_name(const Expression&) const;

    void _open_namespace(const Expression&, std::stringstream&);
    void _close_namespace(const Expression&, std::stringstream&);

    void _echo(const Expression&, std::stringstream&, int);
    void _declare(const Expression&, std::stringstream&, int, bool, bool, std::string);
    void _set(const Expression&, std::stringstream&, int);
    void _call(const Expression&, std::stringstream&, int);
    void _return(const Expression&, std::stringstream&, int);
    void _if(const Expression&, std::stringstream&, int);
    void _do(const Expression&, std::stringstream&, int);
    void _while(const Expression&, std::stringstream&, int);
    void _continue(const Expression&, std::stringstream&, int);
    void _break(const Expression&, std::stringstream&, int);
    void _transaction(const Expression&, std::stringstream&, int);
    void _try(const Expression&, std::stringstream&, int);
    void _throw(const Expression&, std::stringstream&, int);

    void _insert(const Expression&, std::stringstream&, int);
    void _select_content(const Expression&, std::stringstream&, int, bool);
    void _select(const Expression&, std::stringstream&, int, bool);
    void _select(const Expression&, std::stringstream&, int);
    void _update_content(const Expression&, std::stringstream&, int);
    void _update(const Expression&, std::stringstream&, int);
    void _delete_content(const Expression&, std::stringstream&, int);
    void _delete(const Expression&, std::stringstream&, int);

    void _include(const Expression&);
    void _link(const Expression&);
    std::vector<index_desc_t> _table_indexes(const Expression&);
    void _table(const Expression&);
    void _parameters_head(const Expression&, std::stringstream&, int);
    void _parameters_impl(const Expression&, std::stringstream&, int);
    void _parameters_tmpl(const Expression&, std::stringstream&, int);
    void _procedure(const Expression&);
    void _class(const Expression&, bool);
    void _type(const Expression&);
    void _enum(const Expression&);
    void _sequence(const Expression&);

    friend class Zigurat::HeadCompiler;
    friend class Zigurat::ExprCompiler;
    friend class Zigurat::WhereCompiler;
  };
	
}

#endif // __COMPILER_HPP__
