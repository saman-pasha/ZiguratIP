;;; probe.el --- what does parsi-mode want to indent each line to?  -*- lexical-binding: t; -*-

;; The strongest check src/indent.js has: not "does it match the corpus" but
;; "does it match parsi-mode", which is the thing the corpus was written with.
;;
;; Each line is re-indented, the answer recorded, and the line put straight
;; back, so one line's answer never shifts the next line's input -- the same
;; non-cascading measurement test/indent.mjs makes.  Running `indent-region'
;; instead would compound every disagreement down the file and say nothing
;; about any single rule.
;;
;; THE UNIT IS PER FILE, because the tree is split: System/, Test/ and colab/
;; are written with tabs and demo/ with four spaces.  `current-indentation'
;; counts a tab as `tab-width' columns, so tab-width is set equal to
;; `parsi-indent-offset' and one tab is then exactly one level, which is what
;; parsi-indent-offset's own docstring tells you to do.
;;
;; Prints one TSV row per non-blank line: path, line, indent in the file,
;; indent parsi-mode wants.
;;
;;   emacs -Q --batch -l tools/probe.el ../emacs \
;;         $(cd .. && git ls-files '*.parsi' | sed 's|^|../|')

(add-to-list 'load-path (expand-file-name (car command-line-args-left)))
(require 'parsi-mode)
(setq vc-handled-backends nil)

(defun probe--unit (text)
  "Columns per level in TEXT, and whether it is written with tabs."
  (if (string-match "^\\(\t+\\|  +\\)[^ \t\n]" text)
      (let ((lead (match-string 1 text)))
        (if (string-prefix-p "\t" lead) (cons 4 t) (cons (length lead) nil)))
    (cons 4 nil)))

(dolist (f (cdr command-line-args-left))
  (with-temp-buffer
    (insert-file-contents f)
    (let* ((unit (probe--unit (buffer-string))))
      (parsi-mode)
      (setq-local parsi-indent-offset (car unit))
      (setq-local tab-width (car unit))
      (setq-local indent-tabs-mode (cdr unit))
      (goto-char (point-min))
      (while (not (eobp))
        (unless (looking-at "[ \t]*$")
          (let ((was (current-indentation)))
            (indent-according-to-mode)
            (princ (format "%s\t%d\t%d\t%d\n" f (line-number-at-pos)
                           was (current-indentation)))
            (unless (= was (current-indentation))
              (delete-region (line-beginning-position)
                             (progn (skip-chars-forward " \t") (point)))
              (indent-to was))))
        (forward-line 1)))))

;;; probe.el ends here
