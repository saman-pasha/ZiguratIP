// Apply the language-configuration indentation rules to the real corpus and
// compare against how the files are actually indented.
//
// VSCode's rule is not a parser. For each line it does two things:
//   - if decreaseIndentPattern matches THIS line, this line sits one level out
//   - if increaseIndentPattern matched the PREVIOUS line, this line sits one in
// That is all the information a regex pair can carry, so this file is how we
// find out whether it is enough for a language where BEGIN and END share a line
// 124 times across seven files.
//
// Run: node test/indent.mjs
import { readFileSync, globSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
const repo = join(root, '..');

const cfg = JSON.parse(
  readFileSync(join(root, 'language-configuration.json'), 'utf8')
);
// VSCode compiles these as JAVASCRIPT regexes, not Oniguruma -- so (?i) is a
// syntax error here where it is fine in the grammar. The {pattern, flags} form
// is how a language configuration asks for case-insensitivity.
const rx = (r) => (typeof r === 'string' ? new RegExp(r) : new RegExp(r.pattern, r.flags));
const inc = rx(cfg.indentationRules.increaseIndentPattern);
const dec = rx(cfg.indentationRules.decreaseIndentPattern);

// THE CORPUS MIXES TABS AND SPACES -- System/ indents with tabs, demo/ with
// four spaces -- so depth is measured against whatever unit a file itself uses,
// detected from its own first indented line. Counting tabs alone reported every
// space-indented file as a misprediction, which is a fault in the ruler.
function unitOf(text) {
  for (const line of text.split(/\r?\n/)) {
    const m = line.match(/^(\t+|  +)(?=\S)/);
    if (m) return m[1][0] === '\t' ? 1 : m[1].length;
  }
  return 1;
}
const depthOf = (line, unit) => {
  const lead = (line.match(/^[ \t]*/) || [''])[0];
  return unit === 1 ? lead.replace(/ /g, '').length : Math.round(lead.length / unit);
};

/** net parens on a line, once strings and comments are already gone */
const open = (code) =>
  (code.match(/\(/g) || []).length - (code.match(/\)/g) || []).length;

/** strip what the rules must not see, tracking state ACROSS lines.
 *
 * A Parsi string may span lines -- System/catalog_pages.parsi opens one with
 * ECHO ' and writes a table of HTML inside it -- and so may a bracketed
 * parameter list. Both have to be carried between lines or the rules get shown
 * text that is not code, and the ruler blames them for it.
 */
function significantLines(text) {
  const out = [];
  let inBlock = false, inComment = false, inString = null, depth = 0;

  for (const raw of text.split(/\r?\n/)) {
    if (inBlock) {
      if (/^[ \t]*END[ \t]*$/i.test(raw)) inBlock = false;
      out.push(null);
      continue;
    }
    if (!inString && !inComment && /^[ \t]*BEGIN[ \t]+(HPP|CPP)[ \t]*$/i.test(raw)) {
      inBlock = true;
      out.push(null);
      continue;
    }

    const wasOpen = inString !== null || inComment || depth > 0;
    let code = '';
    for (let i = 0; i < raw.length; i++) {
      const c = raw[i];
      if (inComment) {
        if (c === '*' && raw[i + 1] === '/') { inComment = false; i++; }
        continue;
      }
      if (inString) {
        if (c === '\\') { i++; continue; }
        if (c === inString) inString = null;
        continue;
      }
      if (c === '-' && raw[i + 1] === '-') break;          // line comment
      if (c === '/' && raw[i + 1] === '*') { inComment = true; i++; continue; }
      if (c === "'" || c === '"') { inString = c; continue; }
      if (c === '(') depth++;
      else if (c === ')') depth = Math.max(0, depth - 1);
      code += c;
    }

    const stillOpen = inString !== null || inComment || depth > 0;
    out.push(
      code.trim() === '' ? null : { raw, code, wasOpen, stillOpen }
    );
  }
  return out;
}

let files = 0;
let checked = 0;
let wrong = 0;
const examples = [];

for (const p of globSync('**/*.parsi', { cwd: repo }).filter((x) => !x.startsWith('vscode/'))) {
  const text = readFileSync(join(repo, p), 'utf8');
  const lines = significantLines(text);
  const unit = unitOf(text);
  files++;

  // walk with the file's own indentation as the running truth, so one bad
  // prediction does not cascade into every line after it
  for (let i = 1; i < lines.length; i++) {
    const cur = lines[i];
    const prev = lines[i - 1];
    if (!cur || !prev) continue;

    // A CONTINUATION LINE IS NOT A RULE'S BUSINESS. A statement wrapped over
    // several lines is aligned to its open paren by the author, and no
    // increase/decrease pair can predict that -- nor should it try.
    // Also skip the line AFTER one, which returns to base from wherever the
    // author had aligned it, and a comma-continued list -- REQUIRES a,\n  b
    // wraps with no bracket for the tracker to see.
    if (prev.stillOpen || prev.wasOpen || cur.wasOpen) continue;
    if (/,\s*$/.test(prev.code)) continue;
    // and the line after that one, which comes back to base
    let j = i - 2;
    while (j >= 0 && !lines[j]) j--;
    if (j >= 0 && /,\s*$/.test(lines[j].code)) continue;

    const predicted =
      depthOf(prev.raw, unit) + (inc.test(prev.code) ? 1 : 0) - (dec.test(cur.code) ? 1 : 0);
    const actual = depthOf(cur.raw, unit);
    checked++;
    if (predicted !== actual) {
      wrong++;
      if (examples.length < 8)
        examples.push(`${p}:${i + 1}  predicted ${predicted} actual ${actual}\n     prev: ${prev.raw.trim()}\n     cur:  ${cur.raw.trim()}`);
    }
  }
}

console.log(`${files} files, ${checked} line pairs, ${wrong} mispredicted`);
for (const e of examples) console.log('  ' + e);

// The corpus is hand-indented and consistent, and continuation lines are
// excluded above, so the rules should reproduce it exactly. Anything left is a
// rule that does not describe the language.
console.log(
  wrong === 0
    ? '\nparsi indentation: all ok'
    : `\nparsi indentation: ${wrong} mispredicted`
);
process.exit(wrong === 0 ? 0 : 1);
