(defsystem "sha1"
  :description "Minimal SHA-1 (local toolchain shim)."
  :depends-on ("base64")
  :components ((:file "sha1")))
