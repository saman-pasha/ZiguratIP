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

## Indentation

Four rules, and they reproduce the corpus exactly: 22 files, 1017 line pairs,
nothing mispredicted. `test/indent.mjs` is what says so — it applies the same
increase/decrease logic VSCode does and compares against how the files are
really indented.

`BEGIN` opens a block unless the same line closes it again; `END` closes one.
An access label opens and closes one at once, which is what puts `PUBLIC:` back
at the level of its enclosing `BEGIN` while its members sit one in from it.

`END ELSE BEGIN` needs no special case. Both rules fire on it — the line dedents
because it starts with `END`, and the next line indents because it ends with
`BEGIN` — which is the right answer.

Continuation lines are the one thing left out, deliberately: a statement wrapped
over several lines is aligned to its open bracket by the author, and an
increase/decrease pair cannot predict that. The test excludes them rather than
pretending, and it knows about the three kinds Parsi has — bracketed lists,
multi-line `ECHO '…'` strings, and comma-continued `REQUIRES` clauses.

**These are JavaScript regexes**, not Oniguruma. VSCode compiles a grammar with
Oniguruma and a language configuration with JS `RegExp`, so `(?i)` is a syntax
error here where it is fine next door. Case-insensitivity uses the
`{ "pattern": …, "flags": "i" }` form.

## Working on it

```sh
npm install
node tools/build-grammar.mjs   # regenerate syntaxes/parsi.tmLanguage.json
node test/tokenize.mjs         # tokenize the real corpus and assert scopes
```

The grammar JSON is generated and committed. Edit the lists in
`tools/build-grammar.mjs`, not the JSON.
