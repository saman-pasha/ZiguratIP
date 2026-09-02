# The two Lisp systems Cicili needs and nothing ships

`cicili.asd` depends on `sha1` and `base64`, two small systems published
under those names nowhere public. These are copies of the ones in
[cocolog/colab/lisp](https://github.com/saman-pasha/cocolog/tree/master/colab/lisp),
whose README says why they are copied rather than reimplemented: Cicili
derives generated MODULE NAMES from this exact digest, so a different
sha1 would rename every module. The install scripts beside this directory
put them in `~/common-lisp`, where ASDF's default source registry finds
them. Change them there, not here; here is a mirror.
