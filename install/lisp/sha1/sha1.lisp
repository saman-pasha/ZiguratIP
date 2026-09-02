(defpackage :sha1 (:use :common-lisp) (:export #:sha1-digest #:sha1-hex #:sha1-base64))
(in-package :sha1)

(declaim (inline u32 rotl))
(defun u32 (x) (logand x #xFFFFFFFF))
(defun rotl (x n) (u32 (logior (ash x n) (ash x (- n 32)))))

(defun string-to-octets (s)
  (map '(vector (unsigned-byte 8)) #'char-code s))

(defun sha1-digest (input)
  "SHA-1 of a string or octet sequence; answers 20 octets."
  (let* ((msg (if (stringp input) (string-to-octets input)
                  (coerce input '(vector (unsigned-byte 8)))))
         (len (length msg))
         (bitlen (* 8 len))
         ;; padded length: message + 0x80 + zeros + 8-byte length, multiple of 64
         (total (* 64 (ceiling (+ len 9) 64)))
         (m (make-array total :element-type '(unsigned-byte 8) :initial-element 0))
         (h0 #x67452301) (h1 #xEFCDAB89) (h2 #x98BADCFE)
         (h3 #x10325476) (h4 #xC3D2E1F0)
         (w (make-array 80 :element-type '(unsigned-byte 32))))
    (replace m msg)
    (setf (aref m len) #x80)
    (loop for i from 0 below 8
          do (setf (aref m (- total 1 i)) (ldb (byte 8 (* 8 i)) bitlen)))
    (loop for chunk from 0 below total by 64 do
      (loop for i from 0 below 16
            do (setf (aref w i)
                     (logior (ash (aref m (+ chunk (* 4 i))) 24)
                             (ash (aref m (+ chunk (* 4 i) 1)) 16)
                             (ash (aref m (+ chunk (* 4 i) 2)) 8)
                             (aref m (+ chunk (* 4 i) 3)))))
      (loop for i from 16 below 80
            do (setf (aref w i)
                     (rotl (logxor (aref w (- i 3)) (aref w (- i 8))
                                   (aref w (- i 14)) (aref w (- i 16)))
                           1)))
      (let ((a h0) (b h1) (c h2) (d h3) (e h4))
        (loop for i from 0 below 80 do
          (multiple-value-bind (f k)
              (cond ((< i 20) (values (logior (logand b c) (logand (lognot b) d)) #x5A827999))
                    ((< i 40) (values (logxor b c d) #x6ED9EBA1))
                    ((< i 60) (values (logior (logand b c) (logand b d) (logand c d)) #x8F1BBCDC))
                    (t        (values (logxor b c d) #xCA62C1D6)))
            (let ((tmp (u32 (+ (rotl a 5) (u32 f) e k (aref w i)))))
              (setf e d d c c (rotl b 30) b a a tmp))))
        (setf h0 (u32 (+ h0 a)) h1 (u32 (+ h1 b)) h2 (u32 (+ h2 c))
              h3 (u32 (+ h3 d)) h4 (u32 (+ h4 e)))))
    (let ((out (make-array 20 :element-type '(unsigned-byte 8))))
      (loop for i from 0 below 5
            for hv in (list h0 h1 h2 h3 h4)
            do (loop for j from 0 below 4
                     do (setf (aref out (+ (* 4 i) j)) (ldb (byte 8 (* 8 (- 3 j))) hv))))
      out)))

(defun sha1-hex (input)
  (string-downcase (format nil "~{~2,'0X~}" (coerce (sha1-digest input) 'list))))

(defun sha1-base64 (input &optional (encoder #'base64:base64-encode))
  "SHA-1 of INPUT, rendered by ENCODER (an octets -> string function)."
  (funcall encoder (sha1-digest input)))
