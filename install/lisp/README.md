# The two Lisp systems Cicili needs and nothing ships

`cicili.asd` depends on `sha1` and `base64`, two small systems published
under those names nowhere public. They are copied rather than
reimplemented because Cicili derives generated MODULE NAMES from this
exact digest, so a different sha1 would rename every module -- which is
also why they must not be changed. The install scripts beside this
directory put them in `~/common-lisp`, where ASDF's default source
registry finds them.
