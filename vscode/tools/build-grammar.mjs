// Build syntaxes/parsi.tmLanguage.json from the token lists below.
//
// THE LISTS COME FROM THE COMPILER, not from reading source by eye.
// ../emacs/parsi-mode.el records the commands that derive them from
// home/etc/patterns.conf -- which IS the grammar, read at runtime by
// Compiler/parser.cpp -- and test/tokenize.mjs re-runs that derivation and
// fails if this file has drifted from it.
//
// Run: node tools/build-grammar.mjs
import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));

// ---------------------------------------------------------------- the lists

// Every word the grammar matches literally, from the NAME directives in
// patterns.conf. NULL is a constant rather than a keyword: it is a value
// everywhere, and a column modifier in COLUMN_EXTEND as well.
const KEYWORDS = [
  'AS', 'ASC', 'BASE', 'BEGIN', 'BREAK', 'BY', 'CALL', 'CATCH', 'CLASS', 'COLUMN',
  'COMMIT', 'COMMITTED', 'COMPILE', 'CONSTRUCTOR', 'CONTINUE', 'CPP', 'DECLARE', 'DEFAULT',
  'DELETE', 'DESC', 'DESTRUCTOR', 'DO', 'ECHO', 'ELSE', 'END', 'ENUM', 'FROM',
  'FUNCTION', 'GLOBAL', 'HPP', 'IF', 'IN', 'INCLUDE', 'INDEX', 'INHERITS',
  'INITIALIZE', 'INOUT', 'INSERT', 'INTO', 'ISOLATION', 'KEY', 'LEVEL', 'LINK',
  'LOCAL', 'ORDER', 'OUT', 'OVERRIDE', 'PAGE', 'PRIMARY', 'PRIVATE', 'PROCEDURE',
  'PROTECTED', 'PUBLIC', 'PURE', 'READ', 'REPEATABLE', 'REQUIRES', 'RETURN',
  'RETURNS', 'ROLLBACK', 'SELECT', 'SEQUENCE', 'SERIALIZABLE', 'SESSION', 'SET',
  'SNAPSHOT', 'STEP', 'TABLE', 'THREAD', 'THROW', 'TO', 'TRANSACTION', 'TRUNCATE',
  'TRY', 'TYPE', 'UNCOMMITTED', 'UNIQUE', 'UPDATE', 'USING', 'VALUES', 'VIRTUAL',
  'WHERE', 'WHILE',
];

// The operators Parsi spells as words. They are OP directives rather than NAME
// ones in patterns.conf, which is why a list derived from NAME alone misses
// every one -- and they are not decoration: NOT NULL is on most columns in
// System/, and AND joins nearly every WHERE.
const WORD_OPERATORS = ['AND', 'BETWEEN', 'IS', 'LIKE', 'NOT', 'OR'];

// The built-in types, from Type/
const TYPES = [
  'Auto', 'Bool', 'Byte', 'Char', 'Double', 'Float', 'Int', 'Long', 'Object', 'Real',
  'Short', 'String', 'Text', 'Timestamp', 'UByte', 'UInt', 'ULong', 'UShort',
  'Vector', 'Void',
];

const CONSTANTS = ['TRUE', 'FALSE', 'NULL'];

// The words that open a top-level object, followed by its name
const OBJECT_OPENERS = [
  'CLASS', 'PAGE', 'PROCEDURE', 'TABLE', 'SEQUENCE', 'TYPE', 'ENUM', 'INDEX',
];

// Compiler/tokenizer.cpp:17 -- MIXED_OPERATORS, longest first so `<=' is not
// read as `<' then `='
const MIXED_OPERATORS = ['::', '<=', '==', '<>', '>='];

// ------------------------------------------------------------------ helpers

const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const alt = (names) =>
  [...names].sort((a, b) => b.length - a.length).map(esc).join('|');

// CASE-INSENSITIVE, GENUINELY. Tokenizer::name() upper-cases every identifier
// character unless a backtick has been seen, so `select`, `Select` and `SELECT`
// are one token to the compiler. This is the one place the Parsi and Cicili
// grammars must not be symmetrical -- Cicili reads with :preserve and is
// case-SENSITIVE.
const word = (names, scope) => ({
  name: scope,
  match: `(?i)\\b(?:${alt(names)})\\b`,
});

// -------------------------------------------------------------- the grammar

const grammar = {
  $schema:
    'https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json',
  name: 'Parsi',
  scopeName: 'source.parsi',
  fileTypes: ['parsi'],
  patterns: [{ include: '#expression' }],
  repository: {
    expression: {
      patterns: [
        // FIRST, AND IT HAS TO BE FIRST. Everything between BEGIN CPP and its
        // END is C++ taken verbatim by the tokenizer, and a single apostrophe
        // in a C++ comment -- "the pool's lock", which Test/ai/classifier.parsi
        // really contains at line 76 -- otherwise opens a Parsi string that
        // swallows the rest of the block.
        { include: '#verbatim-block' },
        { include: '#comment-line' },
        { include: '#comment-block' },
        { include: '#string' },
        { include: '#double-quoted' },
        { include: '#backtick' },
        { include: '#number' },
        { include: '#object-definition' },
        { include: '#access-label' },
        { include: '#constant' },
        { include: '#type' },
        { include: '#word-operator' },
        { include: '#keyword' },
        { include: '#qualified-name' },
        { include: '#operator' },
      ],
    },

    // The layout rule is the tokenizer's own (Compiler/tokenizer.cpp:150-225):
    // a block ends at a line that is nothing but END. Counting BEGIN and END as
    // a nesting depth cannot work -- C++ has neither word, so there is nothing
    // to count -- and C++ may write END wherever it likes so long as it is
    // never alone on a line.
    'verbatim-block': {
      name: 'meta.embedded.parsi',
      begin: '(?i)^[ \\t]*(BEGIN)[ \\t]+(HPP|CPP)[ \\t]*$',
      beginCaptures: {
        1: { name: 'keyword.control.parsi' },
        2: { name: 'keyword.control.parsi' },
      },
      end: '(?i)^[ \\t]*(END)[ \\t]*$',
      endCaptures: { 1: { name: 'keyword.control.parsi' } },
      contentName: 'meta.embedded.block.cpp',
      patterns: [{ include: 'source.cpp' }],
    },

    'comment-line': {
      name: 'comment.line.double-dash.parsi',
      begin: '--',
      end: '$',
    },

    'comment-block': {
      name: 'comment.block.parsi',
      begin: '/\\*',
      end: '\\*/',
    },

    string: {
      name: 'string.quoted.single.parsi',
      begin: "'",
      end: "'",
      patterns: [{ name: 'constant.character.escape.parsi', match: '\\\\.' }],
    },

    'double-quoted': {
      name: 'string.quoted.double.parsi',
      begin: '"',
      end: '"',
      patterns: [{ name: 'constant.character.escape.parsi', match: '\\\\.' }],
    },

    // A backtick escapes into raw C++ for the name that follows it:
    //   DECLARE _net AS `std::`shared_ptr<`Classifier>;
    // It also switches the tokenizer's upper-casing off, so the name keeps its
    // case -- which is the whole point of writing one.
    backtick: {
      match: '(`)([A-Za-z_][A-Za-z_0-9]*)',
      captures: {
        1: { name: 'keyword.operator.cpp-escape.parsi' },
        2: { name: 'support.type.cpp.parsi' },
      },
    },

    number: {
      name: 'constant.numeric.parsi',
      match: '\\b\\d+(?:\\.\\d+)?\\b',
    },

    // TABLE demo::authors -- the opener, then the name
    'object-definition': {
      match: `(?i)\\b(${alt(OBJECT_OPENERS)})\\s+([A-Za-z_][A-Za-z_0-9]*(?:::[A-Za-z_][A-Za-z_0-9]*)*)`,
      captures: {
        1: { name: 'keyword.control.parsi' },
        2: { name: 'entity.name.type.parsi' },
      },
    },

    // PUBLIC: PRIVATE: PROTECTED: -- an access label, matched as text, which is
    // how parsi-mode.el's indenter recognises them too
    'access-label': {
      match: '(?i)^\\s*(PUBLIC|PRIVATE|PROTECTED)\\s*(:)',
      captures: {
        1: { name: 'storage.modifier.parsi' },
        2: { name: 'punctuation.separator.parsi' },
      },
    },

    constant: word(CONSTANTS, 'constant.language.parsi'),
    type: word(TYPES, 'support.type.parsi'),
    'word-operator': word(WORD_OPERATORS, 'keyword.operator.word.parsi'),
    keyword: word(KEYWORDS, 'keyword.control.parsi'),

    // demo::books::id -- one thing, not three
    'qualified-name': {
      match: '\\b[A-Za-z_][A-Za-z_0-9]*(?:::[A-Za-z_][A-Za-z_0-9]*)+\\b',
      name: 'entity.name.namespace.parsi',
    },

    operator: {
      name: 'keyword.operator.parsi',
      match: `(?:${alt(MIXED_OPERATORS)}|[+\\-*/%&|^<=>])`,
    },
  },
};

writeFileSync(
  join(here, '..', 'syntaxes', 'parsi.tmLanguage.json'),
  JSON.stringify(grammar, null, 2) + '\n'
);
console.log('wrote syntaxes/parsi.tmLanguage.json');
