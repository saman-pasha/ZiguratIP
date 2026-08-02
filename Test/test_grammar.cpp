#include "ztest.hpp"
#include "tokenizer.hpp"
#include "token.hpp"
#include "parser.hpp"
#include "expression.hpp"
#include "utility.hpp"
#include "zexception.hpp"
#include <string>
#include <list>
#include <fstream>

using namespace Zigurat;


namespace
{
  // One Parser for the whole suite, deliberately: the server keeps a single
  // Parser for the life of the process, so reusing one here exercises the same
  // path rather than a fresh object per case.
  Parser& parser()
  {
    static Parser* instance = nullptr;
    if (instance == nullptr) {
      const char* candidates[] = {
	"home/etc/patterns.conf", "../home/etc/patterns.conf", "etc/patterns.conf"
      };
      std::string path = Utility::config_path("patterns.conf");
      for (int i = 0; i < 3 && path.empty(); i++) {
	std::ifstream probe(candidates[i]);
	if (probe.good()) path = candidates[i];
      }
      instance = new Parser(path, false);
    }
    return *instance;
  }

  Expression parse(const std::string& rule, const std::string& source)
  {
    std::list<Token> tokens;
    Tokenizer::tokenize(source, tokens);
    return parser().parse(rule, tokens);
  }

  bool parses(const std::string& rule, const std::string& source)
  {
    try {
      parse(rule, source);
      return true;
    } catch (...) {
      return false;
    }
  }

  // CLAUSE is a loop that runs until it sees END, so a bare statement needs the
  // terminator its enclosing block would normally provide.
  bool clause(const std::string& source)
  {
    return parses("CLAUSE", source + "\nEND");
  }

  size_t clause_count(const std::string& source)
  {
    return parse("CLAUSE", source + "\nEND").args.size();
  }
}


// ---------------------------------------------------------------------------
// The grammar file itself
// ---------------------------------------------------------------------------

ZTEST(Grammar, patterns_file_loads)
{
  ZCHECK_NOTHROW(parser());
  ZCHECK(parses("SUITE", ""));   // an empty compilation unit is legal
}

// The server holds one Parser forever. _eof used to survive between calls, so
// every source after the first parsed to an empty tree without complaining.
ZTEST(Grammar, a_parser_can_be_reused)
{
  const std::string source = "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END";

  for (int round = 0; round < 3; round++) {
    Expression ast = parse("SUITE", source);
    ZCHECK_EQ(ast.args.size(), (size_t)1);
    if (ast.args.size() == 1) ZCHECK_STR(ast.args[0].token.value, "TABLE");
  }
}

// A failed parse must not poison the next one.
ZTEST(Grammar, a_parser_recovers_from_a_syntax_error)
{
  ZCHECK(!parses("SUITE", "TABEL t BEGIN END"));
  ZCHECK(parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END"));
}


// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

ZTEST(Grammar, expression_literals)
{
  ZCHECK(parses("EXPR", "1"));
  ZCHECK(parses("EXPR", "3.5"));
  ZCHECK(parses("EXPR", "'text'"));
  ZCHECK(parses("EXPR", "TRUE"));
  ZCHECK(parses("EXPR", "FALSE"));
  ZCHECK(parses("EXPR", "NULL"));
  ZCHECK(parses("EXPR", "-1"));
}

ZTEST(Grammar, expression_operators)
{
  ZCHECK(parses("EXPR", "1 + 2"));
  ZCHECK(parses("EXPR", "1 - 2"));
  ZCHECK(parses("EXPR", "1 * 2"));
  ZCHECK(parses("EXPR", "1 / 2"));
  ZCHECK(parses("EXPR", "1 + 2 * 3"));
  ZCHECK(parses("EXPR", "(1 + 2) * 3"));
  ZCHECK(parses("EXPR", "a == b"));
  ZCHECK(parses("EXPR", "a <> b"));
  ZCHECK(parses("EXPR", "a < b"));
  ZCHECK(parses("EXPR", "a >= b"));
  ZCHECK(parses("EXPR", "a AND b"));
  ZCHECK(parses("EXPR", "a OR b"));
  ZCHECK(parses("EXPR", "NOT a"));
  ZCHECK(parses("EXPR", "a AND b OR NOT c"));
}

ZTEST(Grammar, between_operator)
{
  ZCHECK(clause("SELECT a FROM t WHERE id BETWEEN 1 AND 5;"));
  ZCHECK(clause("SELECT a FROM t WHERE id BETWEEN a AND b;"));
  // A following AND belongs to the enclosing condition, not to the high bound.
  ZCHECK(clause("SELECT a FROM t WHERE id BETWEEN 1 AND 5 AND x == 2;"));
  ZCHECK(!clause("SELECT a FROM t WHERE id BETWEEN 1;"));
  ZCHECK(!clause("SELECT a FROM t WHERE id BETWEEN AND 5;"));
}

ZTEST(Grammar, between_binds_tighter_than_and)
{
  Expression ast = parse("EXPR", "id BETWEEN 1 AND 12 AND x == 2");

  ZCHECK_EQ(ast.args.size(), (size_t)1);
  if (ast.args.empty()) return;

  // AND at the root, with BETWEEN as its left operand rather than the other
  // way round -- otherwise the high bound swallows the second condition.
  const Expression& root = ast.args[0];
  ZCHECK_STR(root.token.value, "AND");
  ZCHECK_EQ(root.args.size(), (size_t)2);
  if (root.args.size() == 2) {
    ZCHECK_STR(root.args[0].token.value, "BETWEEN");
    ZCHECK_EQ(root.args[0].args.size(), (size_t)3);   // subject, low, high
  }
}

ZTEST(Grammar, like_operator)
{
  ZCHECK(clause("SELECT a FROM t WHERE name LIKE 'abc%';"));
  ZCHECK(clause("SELECT a FROM t WHERE name LIKE '_bc';"));
  ZCHECK(clause("SELECT a FROM t WHERE name LIKE p;"));
  ZCHECK(clause("SELECT a FROM t WHERE name LIKE 'a%' AND id == 1;"));
  ZCHECK(!clause("SELECT a FROM t WHERE name LIKE;"));
}

ZTEST(Grammar, expression_names_and_calls)
{
  ZCHECK(parses("EXPR", "a"));
  ZCHECK(parses("EXPR", "a.b"));
  ZCHECK(parses("EXPR", "a.b.c"));
  ZCHECK(parses("EXPR", "a::b"));          // domain qualified
  ZCHECK(parses("EXPR", "a::b::c"));
  ZCHECK(parses("EXPR", "f()"));
  ZCHECK(parses("EXPR", "f(1, 2)"));
  ZCHECK(parses("EXPR", "a::f(1)"));
  ZCHECK(parses("EXPR", "f<Int>()"));      // template argument
}


// ---------------------------------------------------------------------------
// Clauses
// ---------------------------------------------------------------------------

ZTEST(Grammar, echo_clause)
{
  ZCHECK(clause("ECHO 'hi';"));
  ZCHECK(clause("ECHO 1, 2, TRUE;"));
  ZCHECK(clause("ECHO a + b;"));
}

ZTEST(Grammar, declare_clause)
{
  ZCHECK(clause("DECLARE x AS Int;"));
  ZCHECK(clause("DECLARE x AS Int = 5;"));
  ZCHECK(clause("DECLARE x AS String = 'a';"));
  ZCHECK(clause("DECLARE GLOBAL g AS Long;"));
  ZCHECK(clause("DECLARE x AS demo::my_type;"));
}

ZTEST(Grammar, set_call_and_return)
{
  ZCHECK(clause("SET x = 5;"));
  ZCHECK(clause("SET a.b = 'v';"));
  ZCHECK(clause("CALL f();"));
  ZCHECK(clause("CALL demo::p(1, 'a');"));
  ZCHECK(clause("RETURN;"));
  ZCHECK(clause("RETURN 1;"));
  ZCHECK(clause("RETURN a + b;"));
}

ZTEST(Grammar, conditional_clause)
{
  ZCHECK(clause("IF a BEGIN ECHO '1'; END"));
  ZCHECK(clause("IF a BEGIN ECHO '1'; END ELSE BEGIN ECHO '2'; END"));
  ZCHECK(clause("IF a == 1 BEGIN IF b BEGIN ECHO 'n'; END END"));
}

ZTEST(Grammar, loop_clauses)
{
  ZCHECK(clause("WHILE a BEGIN ECHO '1'; END"));
  // DO is a do-while: the body runs before the condition is tested.
  ZCHECK(clause("DO BEGIN ECHO '1'; END WHILE a;"));
  ZCHECK(clause("WHILE a BEGIN CONTINUE; END"));
  ZCHECK(clause("WHILE a BEGIN BREAK; END"));
}

ZTEST(Grammar, exception_clauses)
{
  ZCHECK(clause("TRY BEGIN ECHO '1'; END CATCH e AS String BEGIN ECHO '2'; END"));
  // THROW takes an object, not an arbitrary expression.
  ZCHECK(clause("THROW error;"));
  ZCHECK(!clause("THROW 'a literal';"));
}

ZTEST(Grammar, transaction_clause)
{
  ZCHECK(clause("TRANSACTION BEGIN ECHO '1'; END"));
  ZCHECK(clause("TRANSACTION BEGIN INSERT INTO t VALUES (1); END"));
}

ZTEST(Grammar, clauses_sequence)
{
  ZCHECK_EQ(clause_count("ECHO '1'; ECHO '2'; ECHO '3';"), (size_t)3);
  ZCHECK_EQ(clause_count(""), (size_t)0);
}


// ---------------------------------------------------------------------------
// Data manipulation
// ---------------------------------------------------------------------------

ZTEST(Grammar, select_statement)
{
  ZCHECK(clause("SELECT id FROM t;"));
  ZCHECK(clause("SELECT id, name FROM t;"));
  ZCHECK(clause("SELECT * FROM t;"));
  ZCHECK(clause("SELECT id FROM demo::t;"));
  ZCHECK(clause("SELECT id FROM t WHERE id == 1;"));
  ZCHECK(clause("SELECT id FROM t WHERE id > 1 AND name == 'a';"));
  ZCHECK(clause("SELECT id FROM t ORDER BY id;"));
  ZCHECK(clause("SELECT id FROM t WHERE id == 1 ORDER BY id;"));
}

// An item written "name = expression" assigns once per row instead of being
// emitted -- SET, in the one place a statement block cannot go.
ZTEST(Grammar, select_assigns_to_a_variable)
{
  ZCHECK(clause("SELECT last_id = id FROM t;"));
  ZCHECK(clause("SELECT total = total + amount FROM t;"));
  ZCHECK(clause("SELECT n = n + 1, s = s + amount FROM t;"));
  ZCHECK(clause("SELECT last_id = id, name FROM t WHERE id > 1;"));
  ZCHECK(clause("SELECT total = total + amount FROM t WHERE id == 1 ORDER BY id;"));

  // "==" still compares, and the WHERE clause is unaffected either way.
  ZCHECK(clause("SELECT id == 1 FROM t;"));
  ZCHECK(clause("SELECT id FROM t WHERE id = 1;"));

  ZCHECK(!clause("SELECT total = FROM t;"));
}

ZTEST(Grammar, insert_statement)
{
  ZCHECK(clause("INSERT INTO t VALUES (1, 'a');"));
  ZCHECK(clause("INSERT INTO demo::t VALUES (1);"));
  ZCHECK(clause("INSERT INTO t VALUES (s::NEXT(), 'a');"));
}

ZTEST(Grammar, update_and_delete_statements)
{
  ZCHECK(clause("UPDATE t SET name = 'b' WHERE id == 1;"));
  ZCHECK(clause("UPDATE demo::t SET a = 1, b = 2 WHERE id == 1;"));
  ZCHECK(clause("DELETE FROM t WHERE id == 1;"));
  ZCHECK(clause("DELETE FROM demo::t;"));
}


// ---------------------------------------------------------------------------
// Schema objects
// ---------------------------------------------------------------------------

ZTEST(Grammar, table_definition)
{
  ZCHECK(parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END"));
  ZCHECK(parses("SUITE", "TABLE demo::t BEGIN COLUMN id AS Long PRIMARY KEY; END"));
  ZCHECK(parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY DEFAULT 0; END"));
  ZCHECK(parses("SUITE",
		"TABLE t BEGIN"
		"  COLUMN id AS Long PRIMARY KEY;"
		"  COLUMN name AS String UNIQUE KEY NOT NULL;"
		"  COLUMN note AS Text NULL;"
		"END"));
  ZCHECK(parses("SUITE",
		"TABLE t BEGIN COLUMN a AS Long; COLUMN b AS Long; PRIMARY KEY (a, b); END"));
  ZCHECK(parses("SUITE",
		"TABLE a::t REQUIRES b::u BEGIN COLUMN id AS Long PRIMARY KEY; END"));
}

ZTEST(Grammar, table_ast_shape)
{
  Expression ast = parse("SUITE", "TABLE demo::t BEGIN COLUMN id AS Long PRIMARY KEY; COLUMN n AS String; END");

  ZCHECK_EQ(ast.args.size(), (size_t)1);
  if (ast.args.empty()) return;

  const Expression& table = ast.args[0];
  ZCHECK_STR(table.token.value, "TABLE");
  // domain, then one node per column.
  ZCHECK_EQ(table.args.size(), (size_t)3);
  if (table.args.size() < 3) return;

  // Names are folded to upper case by the tokenizer, and demo::t nests.
  ZCHECK_STR(table.args[0].token.value, "DEMO");
  ZCHECK_EQ(table.args[0].args.size(), (size_t)1);
  if (!table.args[0].args.empty()) ZCHECK_STR(table.args[0].args[0].token.value, "T");

  ZCHECK_STR(table.args[1].token.value, "COLUMN");
  ZCHECK_EQ(table.args[1].args.size(), (size_t)3);   // name, type, PRIMARY
  ZCHECK_STR(table.args[2].token.value, "COLUMN");
  ZCHECK_EQ(table.args[2].args.size(), (size_t)2);   // name, type
}

ZTEST(Grammar, sequence_definition)
{
  ZCHECK(parses("SUITE", "SEQUENCE s BEGIN FROM 1; TO 100; STEP 1; END"));
  ZCHECK(parses("SUITE", "SEQUENCE demo::s BEGIN FROM 1; TO Long::MAX; STEP 1; END"));
  // All three bounds are required.
  ZCHECK(!parses("SUITE", "SEQUENCE s BEGIN FROM 1; TO 100; END"));
  ZCHECK(!parses("SUITE", "SEQUENCE s BEGIN FROM 1; END"));
}

ZTEST(Grammar, enum_definition)
{
  // Flags are comma separated, with no trailing separator.
  ZCHECK(parses("SUITE", "ENUM e BEGIN a, b, c END"));
  ZCHECK(parses("SUITE", "ENUM demo::e BEGIN EMPLOYEE, MANAGER END"));
  ZCHECK(!parses("SUITE", "ENUM e BEGIN a; b; END"));
}

ZTEST(Grammar, type_definition)
{
  ZCHECK(parses("SUITE", "TYPE t AS Long;"));
  ZCHECK(parses("SUITE", "TYPE demo::id AS Long;"));
}

ZTEST(Grammar, procedure_definition)
{
  ZCHECK(parses("SUITE", "PROCEDURE p() RETURNS Void BEGIN ECHO 'x'; END"));
  ZCHECK(parses("SUITE", "PROCEDURE demo::p() RETURNS Long BEGIN RETURN 1; END"));
  ZCHECK(parses("SUITE", "PROCEDURE p(a AS Int) RETURNS Void BEGIN ECHO 'x'; END"));
  ZCHECK(parses("SUITE", "PROCEDURE p(a AS Int, b AS String) RETURNS Void BEGIN ECHO 'x'; END"));
  ZCHECK(parses("SUITE", "PROCEDURE p(a AS Int IN, b AS Int OUT, c AS Int INOUT) RETURNS Void BEGIN ECHO 'x'; END"));
  ZCHECK(parses("SUITE", "PROCEDURE p(a AS Int = 5) RETURNS Void BEGIN ECHO 'x'; END"));
  ZCHECK(parses("SUITE", "PROCEDURE p() RETURNS Void REQUIRES demo::t BEGIN ECHO 'x'; END"));
}

ZTEST(Grammar, class_definition)
{
  ZCHECK(parses("SUITE", "CLASS c BEGIN PUBLIC: DECLARE x AS Int; END"));
  ZCHECK(parses("SUITE", "CLASS c BEGIN PRIVATE: DECLARE x AS Int; PUBLIC: DECLARE y AS Int; END"));
  ZCHECK(parses("SUITE", "CLASS c BEGIN PROTECTED: DECLARE x AS Int; END"));
  ZCHECK(parses("SUITE", "CLASS c BEGIN PUBLIC: FUNCTION f() RETURNS Void BEGIN ECHO 'x'; END END"));
  ZCHECK(parses("SUITE", "CLASS c BEGIN PUBLIC: FUNCTION f(a AS Int) RETURNS Int BEGIN RETURN a; END END"));
  ZCHECK(parses("SUITE", "CLASS c INHERITS b BEGIN PUBLIC: DECLARE x AS Int; END"));
}

ZTEST(Grammar, page_definition)
{
  ZCHECK(parses("SUITE",
		"PAGE p BEGIN PUBLIC: "
		"OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void BEGIN ECHO 'x'; END "
		"END"));
  ZCHECK(parses("SUITE",
		"PAGE p REQUIRES Session BEGIN PUBLIC: "
		"OVERRIDE FUNCTION PAGE_LOAD() RETURNS Void BEGIN ECHO 'x'; END "
		"END"));
}


// ---------------------------------------------------------------------------
// Compilation units
// ---------------------------------------------------------------------------

ZTEST(Grammar, suite_directives_need_an_object_after_them)
{
  // INCLUDE and LINK introduce a unit; they do not make one on their own.
  ZCHECK(!parses("SUITE", "INCLUDE '<vector>';"));
  ZCHECK(!parses("SUITE", "LINK '-lm';"));

  ZCHECK(parses("SUITE", "INCLUDE '<vector>' ; TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END"));
  ZCHECK(parses("SUITE", "INCLUDE '\"header.hpp\"' ; TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END"));

  Expression ast = parse("SUITE",
			 "INCLUDE '<vector>'; "
			 "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END "
			 "SEQUENCE s BEGIN FROM 1; TO 9; STEP 1; END");
  ZCHECK_EQ(ast.args.size(), (size_t)3);
}

ZTEST(Grammar, several_objects_in_one_unit)
{
  Expression ast = parse("SUITE",
			 "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END "
			 "SEQUENCE s BEGIN FROM 1; TO 9; STEP 1; END "
			 "PROCEDURE p() RETURNS Void BEGIN ECHO 'x'; END");
  ZCHECK_EQ(ast.args.size(), (size_t)3);
  if (ast.args.size() == 3) {
    ZCHECK_STR(ast.args[0].token.value, "TABLE");
    ZCHECK_STR(ast.args[1].token.value, "SEQUENCE");
    ZCHECK_STR(ast.args[2].token.value, "PROCEDURE");
  }
}


// ---------------------------------------------------------------------------
// Malformed input. A grammar is only as good as what it refuses.
// ---------------------------------------------------------------------------

ZTEST(Grammar, rejects_unclosed_blocks)
{
  ZCHECK(!parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY;"));
  ZCHECK(!parses("SUITE", "PROCEDURE p() RETURNS Void BEGIN ECHO 'x';"));
  ZCHECK(!parses("SUITE", "CLASS c BEGIN PUBLIC: DECLARE x AS Int;"));
}

ZTEST(Grammar, rejects_missing_keywords)
{
  ZCHECK(!parses("SUITE", "TABLE t COLUMN id AS Long PRIMARY KEY; END"));   // no BEGIN
  ZCHECK(!parses("SUITE", "PROCEDURE p() BEGIN ECHO 'x'; END"));            // no RETURNS
  ZCHECK(!parses("SUITE", "TABEL t BEGIN COLUMN id AS Long; END"));         // misspelt
  ZCHECK(!clause("SELECT id t;"));                                          // no FROM
  ZCHECK(!clause("INSERT t VALUES (1);"));                                  // no INTO
}

ZTEST(Grammar, rejects_incomplete_declarations)
{
  ZCHECK(!parses("SUITE", "TABLE t BEGIN COLUMN id AS; END"));
  ZCHECK(!parses("SUITE", "TABLE t BEGIN COLUMN AS Long; END"));
  ZCHECK(!parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY END"));  // no ;
  ZCHECK(!clause("DECLARE x AS;"));
  ZCHECK(!clause("SET = 5;"));
}

ZTEST(Grammar, rejects_unbalanced_punctuation)
{
  ZCHECK(!parses("SUITE", "PROCEDURE p( RETURNS Void BEGIN ECHO 'x'; END"));
  ZCHECK(!parses("EXPR", "(1 + 2"));
  ZCHECK(!clause("ECHO 'unterminated;"));
  // Trailing rubbish is caught by the top level rule, which has to reach the
  // end of the token stream.
  ZCHECK(!parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END )"));
}

ZTEST(Grammar, rejects_stray_input)
{
  ZCHECK(!parses("SUITE", ";"));
  ZCHECK(!parses("SUITE", "BEGIN END"));
  ZCHECK(!parses("SUITE", "COLUMN id AS Long;"));    // outside a table
  ZCHECK(!parses("SUITE", "ECHO 'x';"));             // a clause is not a top-level object
}

// Parsing a single rule matches that rule and stops; it does not require the
// whole token stream to be consumed. Only SUITE has to reach the end.
ZTEST(Grammar, a_sub_rule_parse_stops_at_its_own_end)
{
  ZCHECK(parses("EXPR", "1 + 2)"));
  ZCHECK(parses("EXPR", "1 + 2 END"));
  ZCHECK(!parses("SUITE", "TABLE t BEGIN COLUMN id AS Long PRIMARY KEY; END END"));
}

ZTEST(Grammar, syntax_errors_carry_a_position)
{
  bool reported = false;
  try {
    parse("SUITE", "TABLE t BEGIN\n  COLUMN id AS Long PRIMARY KEY\nEND");
  } catch (const ZiguratException& error) {
    // The message names where it gave up rather than just failing.
    const std::string message = error.message();
    reported = (message.find("line") != std::string::npos &&
		message.find("column") != std::string::npos);
  } catch (...) {
  }
  ZCHECK(reported);
}
