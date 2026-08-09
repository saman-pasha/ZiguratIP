# Parsi for VSCode

Syntax highlighting and editor basics for `.parsi` files.

## What it knows

Keywords, types, constants and the word-shaped operators come from the
compiler, not from reading source by eye. `../emacs/parsi-mode.el` records the
derivations from `home/etc/patterns.conf` — which *is* the grammar, read at
runtime by `Compiler/parser.cpp` — and `test/tokenize.mjs` re-runs that
derivation and fails if the lists here have drifted from it. Punctuation
operators come from `Compiler/tokenizer.cpp`.

Highlighting is **case-insensitive**, because the language is: `Tokenizer::name()`
upper-cases every identifier unless a backtick has been seen, so `select`,
`Select` and `SELECT` are one token to the compiler.

## `BEGIN HPP` / `BEGIN CPP`

These hold C++ that the compiler copies through untouched, and the grammar
treats them as an embedded `source.cpp` region. That is the first rule in the
file and it has to be, for a specific reason: a single apostrophe in a C++
comment — "the pool's lock", at `Test/ai/classifier.parsi:76` — otherwise opens
a Parsi string that swallows the rest of the block.

The block ends at a line that is nothing but `END`. That is the tokenizer's own
rule (`Compiler/tokenizer.cpp:150-225`): C++ has neither `BEGIN` nor `END`, so
there is nothing to count, and a layout rule is what can be checked instead.

**One dependency worth naming.** The embedding is `{ "include": "source.cpp" }`,
and vscode-textmate drops the *entire* rule if that scope cannot be resolved.
VSCode bundles the C++ grammar so it resolves there; if it ever did not, the
block would stop being protected. The test registers a stub `source.cpp` to
exercise the rule, so a change that breaks the block boundary still fails here.

## Known limit

Indentation uses `increaseIndentPattern` / `decreaseIndentPattern`, which cannot
count net effect the way `parsi-mode.el` does. `BEGIN` and `END` may share a
line — `END ELSE BEGIN` — and on such a line the indent will be wrong by one
level. Re-indenting the block fixes it.

## Working on it

```sh
npm install
node tools/build-grammar.mjs   # regenerate syntaxes/parsi.tmLanguage.json
node test/tokenize.mjs         # tokenize the real corpus and assert scopes
```

The grammar JSON is generated and committed. Edit the lists in
`tools/build-grammar.mjs`, not the JSON.
