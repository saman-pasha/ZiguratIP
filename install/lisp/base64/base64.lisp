(defpackage :base64 (:use :common-lisp) (:export #:base64-encode))
(in-package :base64)

(defparameter +alphabet+
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")

(defun base64-encode (octets)
  "Encode a sequence of (unsigned-byte 8) as a padded base64 string."
  (let* ((v (coerce octets 'vector))
         (n (length v))
         (out (make-string-output-stream)))
    (loop for i from 0 below n by 3
          for b0 = (aref v i)
          for b1 = (if (< (+ i 1) n) (aref v (+ i 1)) 0)
          for b2 = (if (< (+ i 2) n) (aref v (+ i 2)) 0)
          for triple = (logior (ash b0 16) (ash b1 8) b2)
          do (write-char (char +alphabet+ (ldb (byte 6 18) triple)) out)
             (write-char (char +alphabet+ (ldb (byte 6 12) triple)) out)
             (write-char (if (< (+ i 1) n)
                             (char +alphabet+ (ldb (byte 6 6) triple)) #\=) out)
             (write-char (if (< (+ i 2) n)
                             (char +alphabet+ (ldb (byte 6 0) triple)) #\=) out))
    (get-output-stream-string out)))
