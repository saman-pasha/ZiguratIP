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

The extension indents in code, not with rules.

A VSCode language configuration indents with one lever: `increaseIndentPattern`
puts the next line a level in, `decreaseIndentPattern` puts this line a level
out. That lever describes the easy half of Parsi — `BEGIN` opens, `END` closes,
and `END ELSE BEGIN` even falls out of it correctly. What it cannot describe is
anything that needs to know where a **statement** began rather than what the
line above looked like, and that is most of what makes Parsi awkward:

- a wrapped call aligned under its opening paren by the author, where the line
  *after* it must return to the statement's level and not to the column the
  arguments happened to sit at;
- `REQUIRES a,` and `SELECT obj_id,`, which continue with no bracket at all;
- a multi-line `ECHO '…'` — `System/catalog_pages.parsi` keeps a whole HTML
  table in one string, every byte of which the server will send;
- a `BEGIN CPP` block, which is C++ copied through untouched.

Measured over all 22 files, every line predicted from the text above it
(`node test/indent.mjs`):

| model | mispredicted | |
|---|---|---|
| the increase/decrease pair this extension used to ship | 1.6 % | 22 lines, every one a continuation |
| shipping no `indentationRules` at all | 41.6 % | VSCode's bracket default |
| `src/indent.js` | **0 %** | what ships now |

The 1.6% is not a new discovery so much as the previously excluded case: the
earlier version of this test skipped continuation lines, because a regex pair
genuinely cannot predict them, and reported 0 over what was left. Counting them
is what the number above does.

`src/indent.js` is a port of `../emacs/parsi-mode.el` — the same three
"leave this line alone" cases, the same walk back to the statement, the same
net-`BEGIN`-minus-`END` count over the whole statement rather than over the
previous line. `parsi-mode` reproduces this corpus **exactly** (0 of 1617
lines), so it is a real authority, and the port is checked against it line by
line rather than against the files:

```sh
emacs -Q --batch -l tools/probe.el ../emacs \
      $(cd .. && git ls-files '*.parsi' | sed 's|^|../|')
```

`tools/probe.el` re-indents each line, records the answer and puts the line
straight back, so no line's answer shifts the next line's input. The port and
`parsi-mode` agree on all 1617.

`src/extension.js` registers it as on-type (`Enter`, and the last letter of
`END` / `ELSE` / `CATCH` so a closing line pulls itself back as it is typed),
range and document formatting. `formatOnType` is off by VSCode default, so the
manifest turns it on under `[parsi]` only. It does **not** set `insertSpaces` or
`tabSize`: the tree is split — `System/`, `Test/` and `colab/` are written with
tabs, `demo/` with four spaces — so the indenter takes each file's unit from the
file itself, and formatting the whole corpus produces zero edits.

**Language configuration regexes are JavaScript**, not Oniguruma. VSCode
compiles a grammar with Oniguruma and a language configuration with JS
`RegExp`, so `(?i)` is a syntax error here where it is fine next door. Nothing
in this extension needs it now that the indentation rules are gone, but the
`{ "pattern": …, "flags": "i" }` form is how you would ask.

## Working on it

```sh
npm install
node tools/build-grammar.mjs   # regenerate syntaxes/parsi.tmLanguage.json
npm test                       # tokenize the real corpus and assert scopes,
                               # then the indentation
```

The grammar JSON is generated and committed. Edit the lists in
`tools/build-grammar.mjs`, not the JSON.

`test/indent.mjs` leads with hand-written cases rather than the corpus, on
purpose: 22 files is not many and the awkward shapes are the rare ones. There
are two verbatim blocks in the whole tree and neither holds a C++ `begin` or
`end`, so the guard that keeps C++ out of the block count cannot be checked by
any real file — the test says so in place of pretending otherwise.
