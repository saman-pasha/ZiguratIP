// Does the extension indent Parsi the way Parsi is written?
//
// A VSCode language configuration indents with one lever -- increaseIndentPattern
// puts the next line a level in, decreaseIndentPattern puts this line a level
// out -- and that lever can describe the easy half of Parsi: BEGIN opens, END
// closes. It cannot describe anything that needs to know where a STATEMENT
// began: `END ELSE BEGIN' closing and opening at once, a wrapped call the
// author aligned under its paren, `SELECT obj_id,' running on with no bracket
// at all, or a BEGIN CPP block full of C++ that is not Parsi.
//
// So the extension indents in code (src/indent.js, ported from
// ../emacs/parsi-mode.el) and ships no indentationRules. This file is the
// evidence for that, in two parts:
//
//   1. Targeted cases, written here because THE CORPUS CANNOT REACH THEM.
//      22 files is not many, and the awkward shapes are the rare ones: there
//      are two verbatim blocks in the whole tree and neither holds a C++
//      `begin' or `end'. A corpus this size passing says less than it looks.
//   2. The corpus, every line predicted from the text above it, under four
//      models:
//
//        rules     the increase/decrease pair this extension used to ship
//        none      what VSCode does with no indentationRules -- Enter after a
//                  line ending in an open bracket indents, otherwise the new
//                  line keeps the previous line's indent
//        port      src/indent.js, which is what ships
//        emacs     parsi-mode itself, which reproduces the corpus exactly
//                  (0 of 1617 lines), and which the port is checked against
//                  line by line -- see README.md for the batch command
//
// Run: node test/indent.mjs
import { readFileSync, globSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { analyze, indentFor, indentColumns, unitOf, reindent } = require('../src/indent.js');

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
const repo = join(root, '..');

const cfg = JSON.parse(readFileSync(join(root, 'language-configuration.json'), 'utf8'));

let failed = 0;
const check = (what, got, want) => {
  const ok = got === want;
  if (!ok) failed++;
  console.log(
    `${ok ? 'ok  ' : 'FAIL'} ${what.padEnd(58)} ${ok ? got : `got ${got} want ${want}`}`
  );
};

// ------------------------------------------------------ 1. targeted cases

/** Indent one line of a snippet written with four spaces per level. */
const at = (src, n) => {
  const doc = analyze(src, { offset: 4, tabWidth: 4 });
  return indentFor(doc, n);
};

// BEGIN opens and END closes, which is the part a regex pair also gets right
const plain = `PROCEDURE p
BEGIN
    DECLARE x AS Int;
END`;
check('a body sits one level in', at(plain, 2), 4);
check('and END comes back out', at(plain, 3), 0);

// END ELSE BEGIN: -1 and +1 on one line. A rule that matches BEGIN or END
// cannot say this; the effect has to be counted.
const elseIf = `PROCEDURE p
BEGIN
    IF x = 1 BEGIN
        SET y = 1;
    END ELSE BEGIN
        SET y = 2;
    END
END`;
check('END ELSE BEGIN lands where its IF did', at(elseIf, 4), 4);
check('and still opens a block under it', at(elseIf, 5), 8);
check('the END after it closes back to the IF', at(elseIf, 6), 4);

// An access label sits at the level of the BEGIN that holds it, and its body
// one level in from the label -- as in C++.
const cls = `CLASS demo::c
BEGIN
PUBLIC:
    FUNCTION f() RETURNS Void
    BEGIN
    END
END`;
check('an access label pulls back to its BEGIN', at(cls, 2), 0);
check('and its body sits one level in from it', at(cls, 3), 4);

// A wrapped call is aligned by the author and left alone; the line AFTER it
// comes back to the statement's level, not to the column of the arguments.
const wrapped = `PROCEDURE p
BEGIN
    CALL f(a,
           b);
    SET x = 1;
END`;
check('an aligned continuation is left alone', at(wrapped, 3), null);
check('and the line after it returns to the statement', at(wrapped, 4), 4);

// The second kind of continuation, which has no bracket to detect it by
const comma = `PROCEDURE p
REQUIRES Catalog::Tables::Objects,
         Catalog::Sequences::Objects_obj_id
BEGIN
END`;
check('a comma-continued list is left alone', at(comma, 2), null);
check('and BEGIN after it is back at the statement', at(comma, 3), 0);

// C++ inside a verbatim block is copied through byte for byte, and this mode
// has no opinion about how C++ is laid out. `v.begin()' is not a Parsi BEGIN.
//
// The corpus cannot check this: there are two verbatim blocks in 22 files and
// neither holds a C++ `begin' or `end'. What it also cannot check -- and what
// no constructed case here manages to either -- is codeLineEffect's OWN
// verbatim guard. Deleting that guard moves neither the corpus nor any case
// below, because the statement walk restarts at the block's own END line and
// never spans the C++ at all. It is ported because parsi-mode.el has it, and
// this comment is here so nobody reads the passing suite as proof of it.
const cpp = `PROCEDURE p
BEGIN
BEGIN CPP
    for (auto it = v.begin(); it != v.end(); ++it) { count++; }
END
    SET x = 1;
END`;
check('the BEGIN CPP line is ordinary Parsi', at(cpp, 2), 4);
check('C++ inside the block is left alone', at(cpp, 3), null);
check("the block's own END is left alone too", at(cpp, 4), null);
// 0, not 4: the lone END sits exactly on the region boundary, parsi-mode's
// in-verbatim test is inclusive there, and so the statement that follows
// measures from that END's own column. Checked against Emacs, which says 0.
check('and the line after it measures from that END', at(cpp, 5), 0);

// A string that runs across lines is data the server will send
const str = `PAGE p
BEGIN
    ECHO '<table>
<tr><td>x</td></tr>
</table>';
END`;
check('a line inside a string is left alone', at(str, 3), null);

// -------------------------------------------- 2. no rules are shipped

check('language-configuration.json ships no indentationRules',
  cfg.indentationRules === undefined, true);

// ------------------------------------------------ 3. what actually gets typed

// indentFor answers in columns. reindent turns a column into the whitespace
// that goes in the file, and THAT is where a tab-indented tree gets damaged:
// System/, Test/ and colab/ are written with tabs and demo/ with four spaces,
// so a formatter that rendered every answer as spaces would rewrite half the
// tree on the first Enter.
// line 3 is the one to place; line 2 is what tells the file apart
const tabbed = 'PROCEDURE p\nBEGIN\n\tDECLARE x AS Int;\nDECLARE y AS Int;\nEND';
check('a tab-indented file is re-indented with tabs',
  JSON.stringify(reindent(tabbed, 3, 3, {})),
  JSON.stringify([{ line: 3, had: 0, text: '\t' }]));
check('a space-indented file is not given tabs',
  JSON.stringify(reindent('PROCEDURE p\nBEGIN\n    DECLARE x AS Int;\nDECLARE y AS Int;\nEND', 3, 3, {})),
  JSON.stringify([{ line: 3, had: 0, text: '    ' }]));
check('a file with nothing indented yet follows the editor',
  reindent('PROCEDURE p\nBEGIN\nDECLARE x AS Int;\nEND', 2, 2, { tabSize: 2, insertSpaces: true })[0].text,
  '  ');
check('and its tab setting too',
  reindent('PROCEDURE p\nBEGIN\nDECLARE x AS Int;\nEND', 2, 2, { tabSize: 4, insertSpaces: false })[0].text,
  '\t');

// A file that is already right must produce no edits at all -- a formatter
// that rewrites every line it agrees with turns Format Document into a diff.
let churn = 0;
for (const p of globSync('**/*.parsi', { cwd: repo }).filter((x) => !x.startsWith('vscode/')))
  churn += reindent(readFileSync(join(repo, p), 'utf8'), 0, 1e6, {}).length;
check('formatting the whole corpus changes nothing', churn, 0);

// ------------------------------------------------- 4. against the corpus

// what language-configuration.json used to say
const rx = (r) => (typeof r === 'string' ? new RegExp(r) : new RegExp(r.pattern, r.flags));
const OLD = {
  increaseIndentPattern: {
    pattern: '\\bBEGIN\\b(?!.*\\bEND\\b)|^\\s*(PUBLIC|PRIVATE|PROTECTED)\\s*:',
    flags: 'i',
  },
  decreaseIndentPattern: {
    pattern: '^\\s*END\\b|^\\s*(PUBLIC|PRIVATE|PROTECTED)\\s*:',
    flags: 'i',
  },
};
const inc = rx(OLD.increaseIndentPattern);
const dec = rx(OLD.decreaseIndentPattern);
const opens = new RegExp(
  '(?:' + cfg.brackets.map(([o]) => o.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|') + ')\\s*$'
);

const wrong = { rules: 0, none: 0, port: 0 };
let counted = 0;
let leftAlone = 0;
const examples = [];

const files = globSync('**/*.parsi', { cwd: repo })
  .filter((p) => !p.startsWith('vscode/'))
  .sort();

for (const p of files) {
  const text = readFileSync(join(repo, p), 'utf8');
  const unit = unitOf(text);
  const doc = analyze(text, { offset: unit.width, tabWidth: unit.width });

  for (let n = 1; n < doc.lines.length; n++) {
    const line = doc.lines[n];
    if (line.trim() === '') continue;
    // Inside a verbatim block or a string is nobody's business -- not the
    // rules', not the port's -- so it is out of the comparison entirely
    // rather than scored as a free win for whichever model declines first.
    if (doc.inVerbatim(doc.lineStart[n]) || doc.stringAtBol[n]) continue;
    const prev = doc.lines[n - 1];
    if (prev.trim() === '') continue;

    const actual = indentColumns(line, doc.tabWidth);
    counted++;

    const predRules =
      indentColumns(prev, doc.tabWidth) +
      (inc.test(prev) ? unit.width : 0) -
      (dec.test(line) ? unit.width : 0);
    if (predRules !== actual) wrong.rules++;

    const predNone =
      indentColumns(prev, doc.tabWidth) +
      (opens.test(prev.replace(/--.*$/, '')) ? unit.width : 0);
    if (predNone !== actual) wrong.none++;

    // null is a real answer: "this line is the author's, leave it". It keeps
    // what the file has, so it agrees by construction -- counted separately
    // so the port's number cannot hide behind it.
    const mine = indentFor(doc, n);
    if (mine === null) leftAlone++;
    else if (mine !== actual) {
      wrong.port++;
      if (examples.length < 6)
        examples.push(`${p}:${n + 1}  port ${mine} file ${actual}  ${line.trim().slice(0, 52)}`);
    }
  }
}

const pct = (w) => ((w / counted) * 100).toFixed(1).padStart(5);
console.log(`\n${files.length} .parsi files, ${counted} indentable lines\n`);
console.log(`  rules   ${pct(wrong.rules)}%  mispredicted   (the increase/decrease pair, removed)`);
console.log(`  none    ${pct(wrong.none)}%  mispredicted   (VSCode's bracket default)`);
console.log(`  port    ${pct(wrong.port)}%  mispredicted   (src/indent.js -- what ships)`);
console.log(`          ${String(leftAlone).padStart(5)}   of those lines it deliberately leaves alone\n`);
for (const e of examples) console.log('  ' + e);

// parsi-mode reproduces this corpus exactly, so unlike Cicili's there is no
// floor here to stop short of: the port either matches the files or is wrong.
check('src/indent.js reproduces the corpus', wrong.port, 0);
check('the increase/decrease pair did not', wrong.rules > 0, true);

console.log(failed === 0 ? '\nparsi indentation: all ok' : `\nparsi indentation: ${failed} FAILED`);
process.exit(failed === 0 ? 0 : 1);
