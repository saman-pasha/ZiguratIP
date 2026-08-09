// Indentation for Parsi, ported from ../emacs/parsi-mode.el.
//
// A VSCode language configuration indents with one lever: increaseIndentPattern
// puts the next line a level in, decreaseIndentPattern puts this line a level
// out. That lever can describe the easy half of Parsi -- BEGIN opens, END
// closes -- and test/indent.mjs measures how much of the language is left over.
// What it cannot describe is anything that needs to know where a STATEMENT
// began rather than what the previous line looked like:
//
//   * `END ELSE BEGIN' closes and opens on one line, so a line's effect is
//     +1 and -1 at once and has to be counted, not matched.
//   * a wrapped call is aligned under its opening paren by the author, and the
//     line after it must come back to the statement's level, not to the
//     column the arguments happened to be at.
//   * `REQUIRES a,' / `SELECT obj_id,' continue with no bracket at all.
//   * BEGIN CPP blocks are C++ copied through byte for byte, full of braces
//     and the odd `end', and none of it is Parsi.
//
// parsi-mode.el works all of that out, and this is that algorithm rather than
// a fresh one: same three "leave it alone" cases, same walk back to the
// statement, same net-effect count over the whole statement. Where it and the
// Emacs mode disagree the Emacs mode is right, and tools/probe.el is how that
// gets checked -- line by line over the real corpus, not by reading.
//
// Everything is in COLUMNS, the way Emacs measures, with the file's own
// indentation unit as `offset' and as `tabWidth'. The tree is split: System/,
// Test/ and colab/ are written with tabs, demo/ with four spaces.

const DEFAULT = { offset: 4, tabWidth: 4 };

// `:' and a backtick are symbol constituents in parsi-mode's syntax table --
// demo::books is one name, and `std::`shared_ptr is one name -- so a symbol
// boundary is not \b.
const NOT_SYMBOL = '[A-Za-z0-9_:`]';
const BEGIN_END = new RegExp(`(?<!${NOT_SYMBOL})(BEGIN|END)(?!${NOT_SYMBOL})`, 'gi');
const CLOSES_FIRST = new RegExp(`^[ \\t]*(END|ELSE|CATCH)(?!${NOT_SYMBOL})`, 'i');
const ACCESS_LABEL = /^[ \t]*(PUBLIC|PRIVATE|PROTECTED)[ \t]*:/i;
const OPENS_VERBATIM = /^[ \t]*BEGIN[ \t]+(HPP|CPP)[ \t]*$/i;
const LONE_END = /^[ \t]*END[ \t]*$/i;

/** Column of the first non-blank character, tabs expanded. */
function indentColumns(line, tabWidth) {
  let col = 0;
  for (const c of line) {
    if (c === ' ') col++;
    else if (c === '\t') col += tabWidth - (col % tabWidth);
    else break;
  }
  return col;
}

/**
 * Everything the indenter needs to know about a buffer, computed once.
 *
 * Per character: whether it is code, i.e. not inside a string and not inside a
 * comment. Per line: paren depth and string state at its start, comment state
 * at its end. Plus the verbatim block bounds, as character offsets, because
 * that is how parsi-mode compares them and the comparison is at the boundary.
 */
function analyze(text, opts = DEFAULT) {
  const lines = text.split(/\r?\n/);
  const lineStart = new Array(lines.length);
  const lineEnd = new Array(lines.length);
  {
    let at = 0;
    for (let i = 0; i < lines.length; i++) {
      lineStart[i] = at;
      lineEnd[i] = at + lines[i].length;
      at = lineEnd[i] + 1; // the newline
    }
  }

  // BEGIN HPP / BEGIN CPP ... END. The block ends at a line that is nothing
  // but END, which is the tokenizer's own rule (Compiler/tokenizer.cpp), and
  // an unterminated one runs to the end of the buffer so a block being typed
  // does not leak its quotes into the rest of the file.
  const regions = [];
  for (let i = 0; i < lines.length; i++) {
    if (!OPENS_VERBATIM.test(lines[i])) continue;
    let j = i + 1;
    while (j < lines.length && !LONE_END.test(lines[j])) j++;
    regions.push({
      start: lineEnd[i],
      end: j < lines.length ? lineStart[j] : text.length,
      openLine: i,
      endLine: j,
    });
    i = j;
  }
  const inVerbatim = (pos) => regions.some((r) => pos >= r.start && pos <= r.end);
  // syntax-propertize gives a quote inside a block punctuation syntax over
  // [start, end), so it no longer opens a string -- which is the whole point:
  // Test/ai/classifier.parsi has "the pool's lock" in a C++ comment.
  const quoteIsPunctuation = (pos) =>
    regions.some((r) => pos >= r.start && pos < r.end);

  const isCode = new Uint8Array(text.length); // 1 = neither string nor comment
  const depthAtBol = new Int32Array(lines.length);
  const stringAtBol = new Uint8Array(lines.length);
  const commentAtEol = new Uint8Array(lines.length);

  let depth = 0;
  let string = null; // the opening quote character
  let block = false; // inside /* */

  for (let n = 0; n < lines.length; n++) {
    depthAtBol[n] = depth;
    stringAtBol[n] = string ? 1 : 0;
    const line = lines[n];
    const base = lineStart[n];
    let lineComment = false;

    for (let i = 0; i < line.length; i++) {
      const c = line[i];
      const pos = base + i;
      if (block) {
        if (c === '*' && line[i + 1] === '/') { block = false; i++; }
        continue;
      }
      if (lineComment) continue;
      if (string) {
        if (c === '\\') { i++; continue; }
        if (c === string) string = null;
        continue;
      }
      if (c === '-' && line[i + 1] === '-') { lineComment = true; i++; continue; }
      if (c === '/' && line[i + 1] === '*') { block = true; i++; continue; }
      if ((c === "'" || c === '"') && !quoteIsPunctuation(pos)) { string = c; continue; }
      if (c === '(') depth++;
      else if (c === ')') depth = Math.max(0, depth - 1);
      isCode[pos] = 1;
    }
    commentAtEol[n] = block ? 1 : 0;
    // a `--' comment ends at the newline; a string does not
  }

  return {
    text, lines, lineStart, lineEnd,
    isCode, depthAtBol, stringAtBol, commentAtEol,
    inVerbatim, regions,
    offset: opts.offset,
    tabWidth: opts.tabWidth,
  };
}

/** The previous line that is neither blank, nor wholly a comment, nor ending
 *  inside a block comment. Null at the top of the buffer. */
function previousCodeLine(doc, n) {
  for (let i = n - 1; i >= 0; i--) {
    if (/^[ \t]*$/.test(doc.lines[i])) continue;
    if (/^[ \t]*--/.test(doc.lines[i])) continue;
    if (doc.commentAtEol[i]) continue;
    return i;
  }
  return null;
}

/** Does the code line above line `n` end with a comma?
 *
 *  The second kind of continuation, and it has no bracket to detect it by:
 *  `REQUIRES a,' and `SELECT obj_id,' are lists that simply run on, and
 *  System/catalog.parsi aligns them under the first item. */
function previousLineEndsOpen(doc, n) {
  const p = previousCodeLine(doc, n);
  if (p === null) return false;
  let text = doc.lines[p];
  const at = text.indexOf('--'); // a trailing comment does not change the code
  if (at >= 0) text = text.slice(0, at);
  return /,[ \t]*$/.test(text);
}

/** From line `n`, the line the statement on it began on. */
function statementStart(doc, n) {
  let i = n;
  for (;;) {
    const open =
      doc.depthAtBol[i] > 0 || doc.stringAtBol[i] || previousLineEndsOpen(doc, i);
    if (!open) return i;
    const p = previousCodeLine(doc, i);
    if (p === null) return 0; // parsi--previous-code-line lands on line 1 and stops
    i = p;
  }
}

/**
 * Net blocks opened between character offsets `from` and `to`, and whether the
 * first BEGIN or END in that span is an END.
 *
 * Counted, not assumed: `END ELSE BEGIN' is ordinary Parsi and closes and
 * opens at once, so a rule that matches BEGIN or END cannot describe it.
 */
function codeLineEffect(doc, from, to) {
  let net = 0;
  let startsWithClose = false;
  let seen = false;
  BEGIN_END.lastIndex = from;
  for (let m; (m = BEGIN_END.exec(doc.text)); ) {
    if (m.index >= to) break;
    if (doc.isCode[m.index] && !doc.inVerbatim(m.index)) {
      if (m[1].toUpperCase() === 'BEGIN') net++;
      else {
        net--;
        if (!seen) startsWithClose = true;
      }
      seen = true;
    }
  }
  BEGIN_END.lastIndex = 0;
  return { net, startsWithClose };
}

/**
 * The column line `n` should start at, or NULL for "leave this line alone" --
 * which is a real answer, not a failure, and covers three cases parsi-mode is
 * deliberate about: inside a verbatim block, inside a string, and a
 * continuation line the author has already aligned.
 */
function indentFor(doc, n) {
  const line = doc.lines[n];
  if (line === undefined) return null;
  const bol = doc.lineStart[n];

  const continuation = !!(doc.depthAtBol[n] || previousLineEndsOpen(doc, n));

  // C++ copied through byte for byte: this mode has no opinion about how C++
  // is laid out, and a block is very often pasted in from somewhere else.
  if (doc.inVerbatim(bol)) return null;
  // A Parsi string runs across lines and System/catalog_pages.parsi keeps a
  // whole HTML table in one. Every byte is data the server will send.
  if (doc.stringAtBol[n]) return null;
  // One level in is a convention, not the only one: System/catalog.parsi lines
  // its arguments up under the opening paren. A continuation that has been
  // given an indentation keeps it; only one with none is placed.
  if (continuation && indentColumns(line, doc.tabWidth) > 0) return null;

  let base = 0;
  const closesFirst = CLOSES_FIRST.test(line);
  const isLabel = ACCESS_LABEL.test(line);

  const prev = previousCodeLine(doc, n);
  if (prev !== null) {
    // THE LEVEL COMES FROM THE STATEMENT, NOT FROM THE LINE ABOVE. With a
    // wrapped call above it, the line above is a continuation sitting at
    // whatever column its arguments were aligned to.
    const stmt = statementStart(doc, prev);
    const effect = codeLineEffect(doc, doc.lineStart[stmt], doc.lineEnd[n - 1]);
    base =
      indentColumns(doc.lines[stmt], doc.tabWidth) +
      doc.offset * effect.net +
      // the statement's own indent already paid for the closer it opens with
      (effect.startsWithClose ? doc.offset : 0);
    // a body under an access label sits one level in from it, as in C++
    if (ACCESS_LABEL.test(doc.lines[stmt])) base += doc.offset;
  }

  // A line that begins by closing pulls itself back to the level of what it
  // closes -- which is why `END ELSE BEGIN' lands where its IF did.
  if (closesFirst) base -= doc.offset;
  if (isLabel) base -= doc.offset;
  if (continuation) base += doc.offset;

  return Math.max(base, 0);
}

/** The indentation unit a file is written with, from its own first indented
 *  line. The tree is split -- tabs under System/, four spaces under demo/ --
 *  so this is per file, exactly as parsi-indent-offset's docstring says. */
function unitOf(text) {
  for (const line of text.split(/\r?\n/)) {
    const m = line.match(/^(\t+|  +)(?=\S)/);
    if (m)
      return m[1][0] === '\t'
        ? { tabs: true, width: 4, found: true }
        : { tabs: false, width: m[1].length, found: true };
  }
  // Nothing indented yet: a new file, or one statement long. The caller should
  // follow the editor's own settings rather than this guess.
  return { tabs: false, width: 4, found: false };
}

/**
 * The re-indentation of lines [first, last] of `text`, as plain data:
 * {line, columns, text} per line that needs changing.
 *
 * This lives here rather than in extension.js so that it can be run without
 * the vscode module -- rendering a column as tabs or spaces is where a
 * tab-indented tree gets damaged, and it deserves a test of its own.
 *
 * The tree is split -- System/, Test/ and colab/ are written with tabs, demo/
 * with four spaces -- so a file's own unit wins over the editor's setting
 * whenever it has one to state. A file with nothing indented yet follows the
 * editor, which is what a new file should do.
 */
function reindent(text, first, last, options = {}) {
  const own = unitOf(text);
  const width = own.found ? own.width : options.tabSize || 4;
  const tabs = own.found ? own.tabs : options.insertSpaces === false;
  const doc = analyze(text, { offset: width, tabWidth: width });
  const render = (col) =>
    tabs
      ? '\t'.repeat(Math.floor(col / width)) + ' '.repeat(col % width)
      : ' '.repeat(col);

  const out = [];
  for (let n = first; n <= last && n < doc.lines.length; n++) {
    const line = doc.lines[n];
    if (line.trim() === '') continue;
    const columns = indentFor(doc, n);
    if (columns === null) continue; // the author's, or not Parsi at all
    const have = line.length - line.replace(/^[ \t]*/, '').length;
    const next = render(columns);
    if (line.slice(0, have) === next) continue;
    out.push({ line: n, had: have, text: next });
  }
  return out;
}

module.exports = {
  analyze, indentFor, indentColumns, unitOf, reindent,
  previousCodeLine, statementStart, codeLineEffect,
};
