// Tokenize real .parsi files with the shipped grammar and assert scopes, then
// check the keyword list has not drifted from the compiler's own grammar file.
//
// vscode-textmate is the SAME tokenizer VSCode runs, so what passes here is what
// the editor does. No GUI involved.
//
// Run: node test/tokenize.mjs
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const vsctm = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
const repo = join(root, '..');

const wasm = readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
await oniguruma.loadWASM(
  wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength)
);

// A STUB source.cpp, because the real one is VSCode's and is not on disk here.
//
// It has to be registered rather than left absent: an include that does not
// resolve makes vscode-textmate drop the ENTIRE rule containing it, so without
// this the verbatim block never opens and the apostrophe bug this grammar is
// ordered around comes straight back. Measured, not assumed.
//
// VSCode bundles the C++ grammar, so the include resolves there. What the stub
// lets this file test is the part that is ours: that the block opens on
// `BEGIN CPP', closes on a line that is nothing but END, and that no Parsi rule
// runs in between.
const CPP_STUB = JSON.stringify({
  scopeName: 'source.cpp',
  patterns: [{ name: 'meta.stub.cpp', match: '.+' }],
});

const registry = new vsctm.Registry({
  onigLib: Promise.resolve({
    createOnigScanner: (s) => new oniguruma.OnigScanner(s),
    createOnigString: (s) => new oniguruma.OnigString(s),
  }),
  loadGrammar: async (scope) => {
    if (scope === 'source.parsi')
      return vsctm.parseRawGrammar(
        readFileSync(join(root, 'syntaxes', 'parsi.tmLanguage.json'), 'utf8'),
        'parsi.tmLanguage.json'
      );
    if (scope === 'source.cpp')
      return vsctm.parseRawGrammar(CPP_STUB, 'cpp-stub.json');
    return null;
  },
});

const grammar = await registry.loadGrammar('source.parsi');

function tokenize(text) {
  const out = [];
  let rule = vsctm.INITIAL;
  text.split(/\r?\n/).forEach((line, i) => {
    const r = grammar.tokenizeLine(line, rule);
    for (const t of r.tokens) {
      out.push({ line: i + 1, text: line.slice(t.startIndex, t.endIndex), scopes: t.scopes });
    }
    rule = r.ruleStack;
  });
  return out;
}

let failed = 0;
const check = (what, got, want) => {
  const ok = got === want;
  if (!ok) failed++;
  console.log(
    `${ok ? 'ok  ' : 'FAIL'} ${what.padEnd(56)} ${ok ? got : `got ${got} want ${want}`}`
  );
};
const scoped = (toks, text, scope) =>
  toks.some((t) => t.text === text && t.scopes.some((s) => s.includes(scope)));

const src = (p) => readFileSync(join(repo, p), 'utf8');

// ------------------------------------------------- 1. the schema/page basics

const schema = tokenize(src('demo/01-schema.parsi'));
check('TABLE is a keyword', scoped(schema, 'TABLE', 'keyword.control'), true);
check('the table name is an entity',
  scoped(schema, 'demo::authors', 'entity.name.type'), true);
check('AS is a keyword', scoped(schema, 'AS', 'keyword.control'), true);
check('Long is a type', scoped(schema, 'Long', 'support.type'), true);
check('-- opens a comment',
  schema.some((t) => t.scopes.some((s) => s.includes('comment.line.double-dash'))), true);

const pages = tokenize(src('demo/03-pages.parsi'));
check('PAGE is a keyword', scoped(pages, 'PAGE', 'keyword.control'), true);
check('SELECT is a keyword', scoped(pages, 'SELECT', 'keyword.control'), true);
check("a '…' literal is a string",
  pages.some((t) => t.scopes.some((s) => s.includes('string.quoted.single'))), true);
check('PUBLIC: is an access label', scoped(pages, 'PUBLIC', 'storage.modifier'), true);

// ------------------------------- 2. the case-insensitivity the compiler has

// Tokenizer::name() upper-cases every identifier, so these are one token to the
// compiler and must be one colour in the editor.
for (const spelling of ['SELECT', 'select', 'Select']) {
  const t = tokenize(`PAGE p BEGIN ${spelling} 'x' FROM t; END`);
  check(`${spelling} highlights as a keyword`,
    scoped(t, spelling, 'keyword.control'), true);
}

// ---------------------------- 3. THE CASE THE WHOLE GRAMMAR IS ORDERED FOR

// Test/ai/classifier.parsi:76 really contains "the pool's lock" inside a C++
// comment in a BEGIN CPP block. A grammar that lets the Parsi string rule reach
// in there turns 152 characters of ordinary code into a string literal.
const cls = tokenize(src('Test/ai/classifier.parsi'));

const apostropheLine = cls.filter((t) => t.line === 76);
check("classifier.parsi:76 is inside the embedded C++ block",
  apostropheLine.length > 0 &&
    apostropheLine.every((t) => t.scopes.some((s) => s.includes('meta.embedded.block.cpp'))),
  true);
check("and no token on that line is a Parsi string",
  apostropheLine.some((t) => t.scopes.some((s) => s.includes('string.quoted.single.parsi'))),
  false);

// the block must actually close, at a line that is nothing but END
const afterBlocks = cls.filter(
  (t) => t.line > 76 && !t.scopes.some((s) => s.includes('meta.embedded'))
);
check('the CPP block closes and Parsi resumes after it', afterBlocks.length > 0, true);
check('the file does not end inside the embedded block',
  !cls[cls.length - 1].scopes.some((s) => s.includes('meta.embedded.block.cpp')), true);

// the backtick C++ escape
check('a backtick escape is scoped',
  cls.some((t) => t.scopes.some((s) => s.includes('cpp-escape'))), true);

// ------------------------------------------- 4. every .parsi file tokenizes

import { globSync } from 'node:fs';
const all = globSync('**/*.parsi', { cwd: repo }).filter((p) => !p.startsWith('vscode/'));
check('found the .parsi corpus', all.length >= 20, true);
let runaway = 0;
for (const p of all) {
  const toks = tokenize(src(p));
  if (!toks.length) continue;
  const last = toks[toks.length - 1];
  if (last.scopes.some((s) => s.includes('string.quoted') || s.includes('comment.block'))) {
    console.log(`     runaway string/comment at end of ${p}`);
    runaway++;
  }
}
check('no file ends inside a string or block comment', runaway, 0);

// --------------------------- 5. drift: the keywords still match the compiler

// ../emacs/parsi-mode.el records this derivation; patterns.conf IS the grammar.
const conf = src('home/etc/patterns.conf');
const derived = new Set();
for (const m of conf.matchAll(
  /(?:\{[A-Za-z$]+\})?NAME\??:[ \t]+([A-Za-z_][A-Za-z_0-9]*)/g
)) {
  derived.add(m[1]);
}
// Every shipped word list counts. NULL is deliberately a constant rather than
// a keyword -- it is a value everywhere, and a column modifier in
// COLUMN_EXTEND as well -- so checking `keyword' alone reports it as drift.
const repoJson = JSON.parse(
  readFileSync(join(root, 'syntaxes', 'parsi.tmLanguage.json'), 'utf8')
).repository;
const shipped = new Set(
  ['keyword', 'constant', 'type', 'word-operator']
    .flatMap((k) => repoJson[k].match.match(/[A-Za-z_]{2,}/g) || [])
);
const missing = [...derived].filter((k) => !shipped.has(k)).sort();
if (missing.length) console.log(`     keywords in patterns.conf but not shipped: ${missing.join(' ')}`);
check('the keyword list matches patterns.conf', missing.length, 0);

console.log(failed === 0 ? '\nparsi grammar: all ok' : `\nparsi grammar: ${failed} FAILED`);
process.exit(failed === 0 ? 0 : 1);
