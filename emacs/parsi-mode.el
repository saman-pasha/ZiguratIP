;;; parsi-mode.el --- Major mode for Parsi, ZiguratIP's language  -*- lexical-binding: t; -*-

;; Author: ZiguratIP
;; Keywords: languages
;; Package-Requires: ((emacs "25.1"))

;;; Commentary:

;; Editing support for `.parsi' -- syntax highlighting, indentation, and two
;; commands:
;;
;;   C-c C-c   `parsi-compile'         compile here, with the local parsi
;;   C-c C-r   `parsi-compile-remote'  compile on a running Zigurat
;;   C-c C-f   `parsi-open-config'     open connector.conf (C-u for ziguratip.conf)
;;
;; TWO COMPILERS, AND THEY ARE NOT INTERCHANGEABLE.  `parsi' compiles here,
;; reading ziguratip.conf and writing the .so, the generated header and the
;; catalogue entry into this machine's home tree.  `parsic' reads connector.conf
;; and asks a running server, so the object lands where that server will load it
;; -- and it works when the server is not on this machine at all.
;;
;; An object compiled into a home directory nobody is reading does not answer a
;; request, which is what the remote one is for.  The local one is what you want
;; while finding out whether the code is right at all.
;;
;; The server must allow the remote one: COMPILER/REMOTE_MODE in its
;; ziguratip.conf, off by default.  Refused, it says so and the answer appears
;; in the compilation buffer.
;;
;; Install:
;;
;;   (add-to-list 'load-path "/path/to/ZiguratIP/emacs")
;;   (require 'parsi-mode)
;;
;; Both programs have to be on PATH, or `parsi-local-executable' and
;; `parsi-remote-executable' set to them.  The top-level Makefile builds them
;; into home/bin, and like every ZiguratIP binary they need home/lib on the
;; library path -- see `parsi-remote-executable'.

;;; Code:

(require 'compile)

(defgroup parsi nil
  "Editing Parsi, ZiguratIP's language."
  :group 'languages
  :prefix "parsi-")

(defcustom parsi-indent-offset 4
  "Columns a block body is indented past the line that opened it.
The tree is not consistent about this: demo/ is written with four spaces and
Test/ai with tabs.  Emacs indents with tabs when `indent-tabs-mode' is on, so
set both to match whichever file you are in."
  :type 'integer
  :group 'parsi)

(defcustom parsi-local-executable "parsi"
  "The compiler `parsi-compile' runs, here on this machine.

It reads ziguratip.conf and writes the .so, the generated header and the
catalogue entry into this machine's home tree.  See `parsi-remote-executable'
for the other one, and `parsi-compile' for which to reach for."
  :type 'string
  :group 'parsi)

(defcustom parsi-remote-executable "parsic"
  "The client `parsi-compile-remote' runs, which asks a server to compile.

Both of these are found on PATH by default.  ZiguratIP builds them into
home/bin, which is not usually on one, and they link against the shared
libraries in home/lib, which are not usually on the loader's path either -- so
an absolute path alone may not be enough:

  (setq parsi-local-executable  \"/opt/ZiguratIP/home/bin/parsi\")
  (setq parsi-remote-executable \"/opt/ZiguratIP/home/bin/parsic\")

with LD_LIBRARY_PATH (DYLD_LIBRARY_PATH on macOS) carrying home/lib, or a
wrapper script that sets it.  Both commands say which of the two is missing
rather than reporting the loader's message as a compile failure."
  :type 'string
  :group 'parsi)

;;; ------------------------------------------------------------------
;;; syntax
;;; ------------------------------------------------------------------

;; The keyword list is not written from memory: it is every literal word the
;; grammar matches, taken from home/etc/patterns.conf, which is the file the
;; parser itself reads.  Anything highlighted here is therefore something the
;; compiler actually knows, and a keyword added to the grammar shows up as a
;; word this mode does not colour rather than as a silent disagreement.
(defconst parsi-keywords
  '("AS" "ASC" "BASE" "BEGIN" "BREAK" "BY" "CALL" "CATCH" "CLASS" "COLUMN"
    "COMMIT" "COMMITTED" "CONSTRUCTOR" "CONTINUE" "CPP" "DECLARE" "DEFAULT"
    "DELETE" "DESC" "DESTRUCTOR" "DO" "ECHO" "ELSE" "END" "ENUM" "FROM"
    "FUNCTION" "GLOBAL" "HPP" "IF" "IN" "INCLUDE" "INDEX" "INHERITS"
    "INITIALIZE" "INOUT" "INSERT" "INTO" "ISOLATION" "KEY" "LEVEL" "LINK"
    "LOCAL" "ORDER" "OUT" "OVERRIDE" "PAGE" "PRIMARY" "PRIVATE" "PROCEDURE"
    "PROTECTED" "PUBLIC" "PURE" "READ" "REPEATABLE" "REQUIRES" "RETURN"
    "RETURNS" "ROLLBACK" "SELECT" "SEQUENCE" "SERIALIZABLE" "SESSION" "SET"
    "SNAPSHOT" "STEP" "TABLE" "THREAD" "THROW" "TO" "TRANSACTION" "TRUNCATE"
    "TRY" "TYPE" "UNCOMMITTED" "UNIQUE" "UPDATE" "USING" "VALUES" "VIRTUAL"
    "WHERE" "WHILE")
  "Every word the Parsi grammar matches literally.")

(defconst parsi-types
  '("Auto" "Bool" "Byte" "Char" "Double" "Float" "Int" "Long" "Object" "Real"
    "Short" "String" "Text" "Timestamp" "UByte" "UInt" "ULong" "UShort"
    "Vector" "Void")
  "The built-in types, from Type/.")

(defconst parsi-constants '("TRUE" "FALSE" "NULL"))

;; The words that open an object, and the ones that open a block.  Both are
;; needed by the indenter as well as by font-lock, so they are named once.
(defconst parsi-object-openers
  '("CLASS" "PAGE" "PROCEDURE" "TABLE" "SEQUENCE" "TYPE" "ENUM" "INDEX")
  "Words that introduce a top-level object, followed by its name.")

(defvar parsi-mode-syntax-table
  (let ((table (make-syntax-table)))
    ;; Two comment styles, both real: `--' to end of line and `/* */'.
    ;; Style b is the line comment, so a `-' inside a block comment and a `*'
    ;; inside a line comment do not confuse each other.
    (modify-syntax-entry ?-  ". 12b" table)
    (modify-syntax-entry ?\n "> b"   table)
    (modify-syntax-entry ?/  ". 14"  table)
    (modify-syntax-entry ?*  ". 23"  table)

    ;; A string is single-quoted; double quotes are a literal too.
    (modify-syntax-entry ?'  "\"" table)
    (modify-syntax-entry ?\" "\"" table)
    (modify-syntax-entry ?\\ "\\" table)

    ;; `::' is part of a name -- demo::books is one thing, and a `:' that is
    ;; punctuation splits it in every command that moves by symbol.  The access
    ;; labels (PUBLIC:) survive this: the indenter matches them as text.
    (modify-syntax-entry ?_ "_" table)
    (modify-syntax-entry ?: "_" table)

    ;; A backtick escapes the name after it -- `std::`shared_ptr is C++ reached
    ;; through Parsi -- so it holds the name together rather than ending it.
    (modify-syntax-entry ?` "_" table)

    (modify-syntax-entry ?\( "()" table)
    (modify-syntax-entry ?\) ")(" table)
    table)
  "Syntax table for `parsi-mode'.")

;;; ------------------------------------------------------------------
;;; the verbatim blocks
;;; ------------------------------------------------------------------

;; BEGIN HPP ... END and BEGIN CPP ... END hold C++ that the tokenizer copies
;; through untouched, and it must be kept away from the syntax table.  A single
;; apostrophe in a C++ comment -- "the pool's lock", which Test/ai/classifier
;; really contains -- otherwise opens a Parsi string that swallows everything up
;; to the next apostrophe.  Measured on that file: 152 characters of ordinary
;; code coloured as a string literal, and it is a run of C++ either side of a
;; comment, so nothing about it looks like a quoting mistake to the reader.
;;
;; So the quote characters inside such a block are given punctuation syntax.
;; That is all this does: it stops the corruption without pretending to
;; highlight C++, which would mean running a second major mode over the region
;; and is a bigger thing than this file should be.
(defvar-local parsi--verbatim-cache nil
  "Cons of the buffer tick the bounds were computed at, and the bounds.")

;; CACHED, and the reason is not tidiness.  This is called from
;; `syntax-propertize', and `parsi--code-line-effect' asks it once per BEGIN or
;; END it finds -- while `syntax-ppss' on the same line re-enters
;; `syntax-propertize'.  Uncached, indenting demo/03-pages.parsi did not finish
;; in two minutes.  One scan per buffer modification is enough: a block's bounds
;; cannot change without the buffer changing.
(defun parsi--verbatim-region-bounds ()
  "Bounds of every BEGIN HPP/CPP block in the buffer, as a list of (start . end).
START is just after the opening line, END is the line holding its END."
  (if (and parsi--verbatim-cache
           (eq (car parsi--verbatim-cache) (buffer-chars-modified-tick)))
      (cdr parsi--verbatim-cache)
    (let ((computed (parsi--scan-verbatim-regions)))
      (setq parsi--verbatim-cache (cons (buffer-chars-modified-tick) computed))
      computed)))

(defun parsi--scan-verbatim-regions ()
  "Find the verbatim blocks by scanning.  See `parsi--verbatim-region-bounds'."
  (let ((bounds '()))
    (save-excursion
      (goto-char (point-min))
      (while (re-search-forward "^[ \t]*BEGIN[ \t]+\\(HPP\\|CPP\\)[ \t]*$" nil t)
        (let ((start (point)))
          (if (re-search-forward "^[ \t]*END[ \t]*$" nil t)
              (push (cons start (match-beginning 0)) bounds)
            ;; unterminated: to the end of the buffer, so a block being typed
            ;; does not leak its quotes into the rest of the file
            (push (cons start (point-max)) bounds)
            (goto-char (point-max))))))
    (nreverse bounds)))

(defun parsi-syntax-propertize (start end)
  "Neutralise quotes inside verbatim blocks between START and END."
  (dolist (region (parsi--verbatim-region-bounds))
    (let ((from (max start (car region)))
          (to   (min end   (cdr region))))
      (when (< from to)
        (save-excursion
          (goto-char from)
          (while (re-search-forward "['\"]" to t)
            (put-text-property (match-beginning 0) (match-end 0)
                               'syntax-table (string-to-syntax "."))))))))

(defun parsi-in-verbatim-p (&optional pos)
  "Non-nil when POS (point by default) is inside a BEGIN HPP/CPP block."
  (let ((pos (or pos (point))))
    (seq-some (lambda (r) (and (>= pos (car r)) (<= pos (cdr r))))
              (parsi--verbatim-region-bounds))))

;;; ------------------------------------------------------------------
;;; font-lock
;;; ------------------------------------------------------------------

(defconst parsi-font-lock-keywords
  (list
   ;; An object and its name: CLASS demo::books, PAGE catalog.
   (list (concat "\\_<" (regexp-opt parsi-object-openers t) "\\_>[ \t]+"
                 "\\([A-Za-z_][A-Za-z0-9_:]*\\)")
         '(1 font-lock-keyword-face)
         '(2 font-lock-function-name-face))

   ;; FUNCTION name(, CONSTRUCTOR(, DESTRUCTOR(
   '("\\_<FUNCTION\\_>[ \t]+\\([A-Za-z_][A-Za-z0-9_]*\\)"
     (1 font-lock-function-name-face))
   '("\\_<\\(CONSTRUCTOR\\|DESTRUCTOR\\)\\_>" (1 font-lock-keyword-face))

   ;; DECLARE name AS Type -- the name, so a declaration stands out from a use
   '("\\_<DECLARE\\_>[ \t]+\\([A-Za-z_][A-Za-z0-9_]*\\)"
     (1 font-lock-variable-name-face))

   ;; the access labels, which are the one place a colon is not part of a name
   '("^[ \t]*\\(PUBLIC\\|PRIVATE\\|PROTECTED\\)[ \t]*:" (1 font-lock-keyword-face))

   ;; A backticked name is raw C++ reached from Parsi, and worth looking
   ;; different from Parsi's own names -- it is not checked the same way.
   '("`[A-Za-z_][A-Za-z0-9_:`]*" . font-lock-preprocessor-face)

   (cons (concat "\\_<" (regexp-opt parsi-constants t) "\\_>")
         'font-lock-constant-face)
   (cons (concat "\\_<" (regexp-opt parsi-types t) "\\_>")
         'font-lock-type-face)
   (cons (concat "\\_<" (regexp-opt parsi-keywords t) "\\_>")
         'font-lock-keyword-face)

   ;; `this' is the only lower-case word the language gives you
   '("\\_<this\\_>" . font-lock-builtin-face)

   ;; numbers, with the suffixes the tree actually uses: 0L, 137L, 0ul
   '("\\_<[0-9]+\\(\\.[0-9]+\\)?\\([LlUu]+\\)?\\_>" . font-lock-constant-face))
  "Font-lock rules for `parsi-mode'.")

;;; ------------------------------------------------------------------
;;; indentation
;;; ------------------------------------------------------------------

;; BEGIN opens and END closes, and BOTH CAN SHARE A LINE WITH OTHER WORDS --
;; `IF cond BEGIN' and `END ELSE BEGIN' are both ordinary Parsi, the second
;; closing and opening at once.  So a line's effect is counted, not assumed:
;; the level after it is the level before it plus its BEGINs minus its ENDs.
;;
;; What is not counted is a BEGIN or END inside a comment, a string, or a
;; verbatim block -- the C++ in a CPP block is full of braces and the odd
;; `end', and none of it is Parsi.
(defun parsi--code-line-effect (line-start line-end)
  "Net blocks opened between LINE-START and LINE-END.
Answers (NET . STARTS-WITH-CLOSE), the second being non-nil when the first
BEGIN or END on the line is an END."
  (let ((net 0)
        (starts-with-close nil)
        (seen-anything nil))
    (save-excursion
      (goto-char line-start)
      (while (re-search-forward "\\_<\\(BEGIN\\|END\\)\\_>" line-end t)
        (let* ((word  (match-string-no-properties 1))
               (from  (match-beginning 0))
               (to    (match-end 0))
               ;; SYNTAX-PPSS MOVES POINT to the position it is asked about,
               ;; which is behind the match this loop just made.  Left to it,
               ;; the search starts over from there and matches the same BEGIN
               ;; forever -- the loop never terminates and Emacs hangs on the
               ;; first indent.  Both calls are fenced, and the scan is put back
               ;; where the match ended rather than trusted to stay.
               (state    (save-excursion (syntax-ppss from)))
               (verbatim (save-excursion (parsi-in-verbatim-p from))))
          ;; not in a string, not in a comment, not in a verbatim block
          (unless (or (nth 3 state) (nth 4 state) verbatim)
            (if (string= word "BEGIN")
                (setq net (1+ net))
              (setq net (1- net))
              (unless seen-anything (setq starts-with-close t)))
            (setq seen-anything t))
          (goto-char to))))
    (cons net starts-with-close)))

(defun parsi--previous-line-ends-open-p ()
  "Non-nil when the previous code line ends with a comma.

THE SECOND KIND OF CONTINUATION, and it has no bracket to detect it by:

    REQUIRES Catalog::Tables::Objects,
             Catalog::Sequences::Objects_obj_id

    SELECT obj_id,
           obj_type,

Both are lists that simply run on, and System/catalog.parsi aligns them under
the first item.  Judged by paren depth alone they are ordinary lines, and got
flattened to the statement's own column."
  (save-excursion
    (when (parsi--previous-code-line)
      (let ((text (buffer-substring-no-properties
                   (line-beginning-position) (line-end-position))))
        ;; a trailing comment does not change what the code ends with
        (when (string-match "--.*\\'" text)
          (setq text (substring text 0 (match-beginning 0))))
        (string-match-p ",[ \t]*\\'" text)))))

(defun parsi--continuation-p (line-end)
  "Non-nil when the line ending at LINE-END leaves a bracket open.

WHAT A CONTINUATION IS, and it is worth being exact because the first attempt
was not: a line with an unclosed paren at its end.  Nothing else.

Guessing from punctuation instead -- a line not ending in `;\' -- looked right
and was wrong the moment it met `WHILE i < rows\' with its BEGIN on the next
line, which ends in nothing at all and continues nothing.  An open bracket is
the only thing that actually says a statement is unfinished, and `syntax-ppss\'
already tracks it through strings and comments."
  (> (car (syntax-ppss line-end)) 0))

(defun parsi--previous-code-line ()
  "Move to the previous line that is neither blank nor wholly a comment.
Answers nil at the top of the buffer."
  (let ((found nil))
    (while (and (not found) (zerop (forward-line -1)))
      (beginning-of-line)
      (unless (or (looking-at "[ \t]*$")
                  (looking-at "[ \t]*--")
                  (nth 4 (syntax-ppss (line-end-position))))
        (setq found t)))
    found))

(defun parsi--goto-statement-start ()
  "From the current line, move to the line the statement on it began on.
A line inside an open bracket continues the one before it; this walks back to
the first line that is not."
  (while (and (or (> (car (syntax-ppss (line-beginning-position))) 0)
                  ;; A LINE THAT BEGINS INSIDE A STRING is in the middle of one
                  ;; too.  System/catalog_pages.parsi ECHOes a whole HTML table
                  ;; as one literal, and the statement that started it is a
                  ;; dozen lines back -- stopping at the line that merely closed
                  ;; the quote took the markup's own indentation for the
                  ;; statement's, and pushed the SELECT after it out by four
                  ;; levels.
                  (nth 3 (syntax-ppss (line-beginning-position)))
                  (parsi--previous-line-ends-open-p))
              (parsi--previous-code-line))))

(defun parsi-indent-line ()
  "Indent the current line of Parsi."
  (interactive)
  (let* ((position (point-marker))
         (bol (line-beginning-position))
         (this-line (buffer-substring-no-properties bol (line-end-position)))
         (continuation (or (> (car (syntax-ppss bol)) 0)
                           (save-excursion (goto-char bol)
                                           (parsi--previous-line-ends-open-p)))))
    (cond
     ;; INSIDE A VERBATIM BLOCK NOTHING IS TOUCHED.  It is C++ that the
     ;; tokenizer copies through byte for byte, and this mode has no opinion
     ;; about how C++ is laid out -- re-indenting it by Parsi's rules would
     ;; damage a block that is very often pasted in from somewhere else.
     ((parsi-in-verbatim-p bol) nil)

     ;; NOR INSIDE A STRING.  Parsi strings run across lines -- the tokenizer
     ;; wraps them into a C++ raw literal -- and System/catalog_pages.parsi
     ;; keeps a whole HTML table in one.  Every byte of that is data the server
     ;; will send, so indenting it changes what the page looks like, and the
     ;; markup's own nesting has nothing to do with Parsi's.
     ((nth 3 (syntax-ppss bol)) nil)

     ;; ALIGNMENT INSIDE AN OPEN BRACKET IS THE AUTHOR'S.  One level in is a
     ;; convention, not the only one: System/catalog.parsi lines its arguments
     ;; up under the opening paren, tabs and spaces mixed to hit the column, and
     ;; a computed level destroys exactly what that was for.  So a continuation
     ;; that has been given an indentation keeps it, and only one with none --
     ;; a line just typed -- is placed, one level in from the statement.
     ((and continuation (> (current-indentation) 0)) nil)

     (t
      (let ((base 0)
            (closes-first
             (string-match-p "\\`[ \t]*\\(END\\|ELSE\\|CATCH\\)\\_>" this-line))
            (is-label
             (string-match-p "\\`[ \t]*\\(PUBLIC\\|PRIVATE\\|PROTECTED\\)[ \t]*:"
                             this-line)))
        (save-excursion
          (goto-char bol)
          (when (parsi--previous-code-line)
            ;; THE LEVEL COMES FROM THE STATEMENT, NOT FROM THE LINE ABOVE.
            ;; With a wrapped call above it, the line above is a continuation
            ;; sitting at whatever column its arguments were aligned to, and
            ;; measuring from that carried the alignment into the rest of the
            ;; block -- in System/catalog.parsi it pushed RETURNS and every
            ;; line after it out by six tabs.
            (parsi--goto-statement-start)
            (let* ((statement-bol (line-beginning-position))
                   (statement-indent (current-indentation))
                   (statement-line (buffer-substring-no-properties
                                    statement-bol (line-end-position)))
                   ;; over the WHOLE statement, so a BEGIN on a continued line
                   ;; still counts
                   (effect (parsi--code-line-effect
                            statement-bol
                            (save-excursion
                              (goto-char bol)
                              (forward-line -1)
                              (line-end-position)))))
              (setq base (+ statement-indent
                            (* parsi-indent-offset (car effect))
                            ;; the statement's own indent already paid for the
                            ;; closer it opens with -- see `parsi--code-line-effect'
                            (if (cdr effect) parsi-indent-offset 0)))
              ;; a body under an access label sits one level in from it, the way
              ;; it does in C++
              (when (string-match-p
                     "\\`[ \t]*\\(PUBLIC\\|PRIVATE\\|PROTECTED\\)[ \t]*:"
                     statement-line)
                (setq base (+ base parsi-indent-offset))))))

        ;; A line that begins by closing pulls itself back to the level of what
        ;; it closes -- which is why `END ELSE BEGIN' lands where its IF did and
        ;; still leaves the line after it indented.
        (when closes-first (setq base (- base parsi-indent-offset)))
        ;; and a label sits at the level of the BEGIN that holds it
        (when is-label (setq base (- base parsi-indent-offset)))
        ;; a continuation with no indentation yet: one level in
        (when continuation (setq base (+ base parsi-indent-offset)))

        (setq base (max base 0))
        (unless (= base (current-indentation))
          (goto-char bol)
          (skip-chars-forward " \t")
          (delete-region bol (point))
          (goto-char bol)
          (indent-to base)))))
    (when (> (marker-position position) (point))
      (goto-char position))
    (set-marker position nil)))

;;; ------------------------------------------------------------------
;;; the two commands
;;; ------------------------------------------------------------------

;; The same three places, in the same order, that Utility::config_path looks --
;; Core/utility.cpp.  It is asked for rather than reimplemented when the client
;; is available (`parsic --config' prints exactly this), and this is the
;; fallback for when it is not, so the command still answers something useful
;; on a machine with only a checkout.
(defun parsi--config-candidates (name)
  "Where NAME is looked for, most specific first.

The same three places, in the same order, that Utility::config_path walks --
Core/utility.cpp.  Both configuration files are found this way, which is why
this takes the name rather than knowing one."
  (let ((home (getenv "ZIGURATIP_HOME"))
        (candidates '()))
    (when (and home (not (string= home "")))
      (push (expand-file-name (concat "etc/" name) home) candidates))
    (push (expand-file-name (concat "~/ZiguratIP/etc/" name)) candidates)
    (push (if (memq system-type '(windows-nt cygwin))
              (expand-file-name (concat "ZiguratIP/" name)
                                (or (getenv "PROGRAMDATA") "C:/ProgramData"))
            (concat "/etc/ZiguratIP/" name))
          candidates)
    (nreverse candidates)))

(defun parsi--config-path (name)
  "The NAME that would be used, or nil.

For connector.conf the client is asked first -- `parsic --config' prints
exactly the file it will connect with -- so the answer stays right even if the
search order changes underneath this file.  ziguratip.conf has no such
question to ask: `parsi' prints its configuration path only while compiling,
and running a compile to find out where the settings live is not a trade worth
making, so that one is searched here."
  (or (when (string= name "connector.conf")
        (let ((program (executable-find parsi-remote-executable)))
          (when program
            (with-temp-buffer
              (when (zerop (call-process program nil t nil "--config"))
                (let ((line (string-trim (buffer-string))))
                  (and (not (string= line "")) (file-readable-p line) line)))))))
      (seq-find #'file-readable-p (parsi--config-candidates name))))

;;;###autoload
(defun parsi-open-config (&optional server)
  "Open the configuration a compile would use.

Without a prefix argument, connector.conf -- what `parsi-compile-remote\' reads
to decide which server to ask.  Change the host, the port or TLS_MODE here and
the next remote compile goes wherever it now says.

With a prefix argument (\\[universal-argument]), ziguratip.conf instead --
what `parsi-compile\' reads, and the server too.  That is where COMPILER/CPP
and its flags live, where the catalogue and library paths are set, and where
COMPILER/REMOTE_MODE is turned on for the other command to have anything to
talk to.

THE TWO PAIR WITH THE TWO COMPILE COMMANDS, which is the whole reason both are
reachable from one key: whichever compile you are about to run, its settings
are here.

This opens the file itself, not a copy."
  (interactive "P")
  (let* ((name (if server "ziguratip.conf" "connector.conf"))
         (path (parsi--config-path name)))
    (if path
        (find-file path)
      ;; Naming where it looked is the whole value of the message -- "not
      ;; found" alone leaves the reader guessing which of three to create.
      (user-error "No %s in any of: %s" name
                  (string-join (parsi--config-candidates name) ", ")))))

;; What both commands do once they know which program to run.  Saving first is
;; not a convenience: the compiler reads the FILE, so an unsaved buffer compiles
;; the previous version and reports success about code that is not on screen.
(defun parsi--run-compiler (program variable default)
  "Save this buffer and run PROGRAM on its file.

VARIABLE is the setting to name when PROGRAM cannot be found, and DEFAULT the
binary it should point at -- the message says where to look, not what was tried
and failed, which the reader already knows."
  (unless buffer-file-name
    (user-error "Buffer is not visiting a file; save it first"))
  (unless (executable-find program)
    (user-error "Cannot find %s -- set `%s' to ZiguratIP's home/bin/%s"
                program variable default))
  (when (buffer-modified-p)
    (save-buffer))
  ;; from the file's own directory, so the name in a diagnostic is relative and
  ;; compilation-mode resolves the link against the right place
  (let ((default-directory (file-name-directory buffer-file-name)))
    (compile (concat (shell-quote-argument (executable-find program))
                     " "
                     (shell-quote-argument
                      (file-name-nondirectory buffer-file-name))))))

;;;###autoload
(defun parsi-compile ()
  "Compile this buffer HERE, with the local `parsi'.

It reads ziguratip.conf and writes the .so, the generated header and the
catalogue entry into this machine's home tree.  Nothing is sent anywhere.

WHICH OF THE TWO YOU WANT.  This one when the server reading that home tree is
on this machine, or when you are compiling to see whether the code is right at
all and do not need it served.  `parsi-compile-remote' when the server is
elsewhere, or when you want the object where a running server will load it --
an object compiled into a home directory nobody is reading does not answer a
request.

Output goes to a compilation buffer; an error naming a line is a link to it."
  (interactive)
  (parsi--run-compiler parsi-local-executable "parsi-local-executable" "parsi"))

;;;###autoload
(defun parsi-compile-remote ()
  "Compile this buffer on a running Zigurat, through the connector.

Uses the connection in connector.conf -- `parsi-open-config' opens it.  The
object is built on the SERVER, so it lands where that server will load it, and
this works when the server is not on this machine at all.

The server must allow it: COMPILER/REMOTE_MODE in its ziguratip.conf, off by
default.  Refused, the answer says so and it appears here.

Output goes to a compilation buffer; an error naming a line is a link to it."
  (interactive)
  (parsi--run-compiler parsi-remote-executable "parsi-remote-executable" "parsic"))

;; NO ERROR RULE IS ADDED, and that is not an omission.  Both programs print
;;
;;     demo/01-schema.parsi:4:3: syntax error at line 4 column 3 near 'RETRN'
;;
;; which is the GNU form compilation-mode already parses -- checked, and the
;; line does get a `compilation-message' property, so C-x ` and clicking both
;; land on the column.
;;
;; A failure with no position in it -- the server refusing to compile at all,
;; or a file that cannot be read -- is printed as "name: ..." and stays plain
;; text.  A rule for it was written
;; and then removed: an entry whose FILE field is nil never matches, because
;; compile.el has nowhere to jump to, so it sat in the alist doing nothing.
;; What marks that run as failed is the exit status, which parsic sets and the
;; mode line reports.

;;; ------------------------------------------------------------------
;;; the mode
;;; ------------------------------------------------------------------

(defvar parsi-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "C-c C-c") #'parsi-compile)
    (define-key map (kbd "C-c C-r") #'parsi-compile-remote)
    (define-key map (kbd "C-c C-f") #'parsi-open-config)
    map)
  "Keymap for `parsi-mode'.")

;;;###autoload
(define-derived-mode parsi-mode prog-mode "Parsi"
  "Major mode for editing Parsi, ZiguratIP's language.

\\{parsi-mode-map}"
  :syntax-table parsi-mode-syntax-table
  (setq-local font-lock-defaults '(parsi-font-lock-keywords nil nil nil nil))
  (setq-local syntax-propertize-function #'parsi-syntax-propertize)
  (setq-local indent-line-function #'parsi-indent-line)
  (setq-local comment-start "-- ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "\\(--+\\|/\\*+\\)[ \t]*")
  ;; `::' inside a name would otherwise end a sentence for `forward-sentence'
  ;; and friends; the tree writes prose in `--' comments and wants the normal
  ;; paragraph commands to work there.
  (setq-local parse-sexp-ignore-comments t))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.parsi\\'" . parsi-mode))

(provide 'parsi-mode)

;;; parsi-mode.el ends here
