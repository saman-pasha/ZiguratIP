// Indentation for Parsi, as formatting providers.
//
// Highlighting is declarative -- a grammar and a language configuration, no
// code. Indentation is not: a language configuration's increase/decrease pair
// gets BEGIN and END right and everything else wrong, and the everything else
// is the interesting part (test/indent.mjs). src/indent.js is parsi-mode.el's
// algorithm, and it reproduces the corpus exactly.
//
// Three providers:
//
//   on type, "\n"   pressing Enter, which is the one that matters
//   on type         the last letter of each word that pulls a line back --
//                   END, ELSE, CATCH -- and the `:' of an access label, so a
//                   line that closes a block moves as it is typed, the way a
//                   `}' does in a C-like language
//   range/document  Format Selection and Format Document
//
// onTypeFormatting fires on characters an extension names, so the closing
// words have to be spelled out by their last letter, in both cases: D E H and
// `:'. That means a reindent runs on any word ending in `e'; the work is one
// pass over the document, and this file is otherwise a thin adapter, so all
// the interesting behaviour is in src/indent.js where it can be tested.
//
// A line the indenter declines to place -- inside a verbatim block, inside a
// string, or a continuation the author has aligned -- is left exactly as it
// is. That is a decision of parsi-mode's, not a gap: the C++ in a BEGIN CPP
// block is very often pasted in from somewhere else, an ECHO'd HTML table is
// data the server will send byte for byte, and arguments lined up under an
// opening paren are the author's layout.

const vscode = require('vscode');
const { reindent } = require('./indent.js');

const LANG = { language: 'parsi' };

const edits = (document, first, last, options) =>
  reindent(document.getText(), first, last, options).map((e) =>
    vscode.TextEdit.replace(new vscode.Range(e.line, 0, e.line, e.had), e.text)
  );

function activate(context) {
  context.subscriptions.push(
    vscode.languages.registerOnTypeFormattingEditProvider(
      LANG,
      {
        provideOnTypeFormattingEdits(document, position, ch, options) {
          return edits(document, position.line, position.line, options);
        },
      },
      '\n', 'd', 'D', 'e', 'E', 'h', 'H', ':'
    ),
    vscode.languages.registerDocumentRangeFormattingEditProvider(LANG, {
      provideDocumentRangeFormattingEdits(document, range, options) {
        return edits(document, range.start.line, range.end.line, options);
      },
    }),
    vscode.languages.registerDocumentFormattingEditProvider(LANG, {
      provideDocumentFormattingEdits(document, options) {
        return edits(document, 0, document.lineCount - 1, options);
      },
    })
  );
}

function deactivate() {}

module.exports = { activate, deactivate };
