# A neural network as a Zigurat object

What is here is one path, end to end: a network described in Cicili, emitted as
C++, spliced into a Parsi `CLASS` through `BEGIN HPP` and `BEGIN CPP`, compiled
standalone into a `.so`, and reached by a Zeytun page over HTTP.

```
Test/run-ai-e2e.sh
```

## The files

| | |
|---|---|
| `classifier.parsi` | the object and its page — the only hand-written file |
| `classifier.hpp` | declarations, from Cicili. Goes in `BEGIN HPP` |
| `classifier.cpp` | the network and three functions, from Cicili. Goes in `BEGIN CPP` |
| `torch_stub.hpp` | a stand-in for `<torch/torch.h>`, so this runs with nothing installed |

`classifier.{hpp,cpp}` are **generated**. They come from Cicili's
`example/mnist-fragment.cicili`, which describes the network in four lines:

```lisp
(network Classifier
  (input   784)
  (dense   256 relu)
  (dense   128 relu)
  (dense    10 log-softmax))
```

— the same four as its `example/mnist-dsl.cicili`, which trains this network
against the real MNIST. They are committed here so the suite runs without a
Cicili checkout. Point `CICILI_ROOT` at one and the runner re-emits them and
**diffs**, so the generator drifting away from this copy is a failure rather
than a surprise.

## Why the two blocks

`classifier.hpp` is a forward declaration and three prototypes; `classifier.cpp`
is the network itself. Every Zigurat object that `REQUIRES Demo::Classifier`
includes its header, so anything in `HPP` is inherited by all of them, while
`CPP` stays in this object's own translation unit.

That is why `#include <torch/torch.h>` sits **inside the `CPP` block** rather
than in an object-level `INCLUDE`. Written as an `INCLUDE` it would land in the
object's header and every page would compile torch too. Where it is, a page
inherits the **linkage** — `links()` carries it through `REQUIRES` — without
paying for the header. `run-ai-e2e.sh` checks both halves of that on the
generated files rather than taking it on trust.

## The stub, and why there is a copy of it here

`torch_stub.hpp` is a copy of Cicili's `test/cpp/torch_stub.hpp`. A test that
reaches into a sibling checkout is not a test — it passes or fails depending on
what is next to it on disk — so the copy is deliberate.

It is a stand-in, not a library. Its `Linear` is not trained and answers an
arbitrary deterministic function of its input. **Nothing here asserts an
accuracy and nothing should.** What the assertions cover is the plumbing: that
the page answers, that the model reports its own input width, that the same
input twice agrees, and that two different inputs do not collapse to one
answer — the last being the one that catches a model nothing ever reaches.

For the real thing:

```
pip install torch --target /opt/torch
TORCH_ROOT=/opt/torch/torch Test/run-ai-e2e.sh
```

The same `classifier.parsi` is used either way; the runner rewrites the one
include line and adds the `LINK` flags.

## Two things that bit while this was written

**A quoted include finds `home/tmp` first.** The generated
`_DEMO::CLASSIFIER_.cpp` is compiled in `home/tmp`, so `#include
"torch_stub.hpp"` looks there before anywhere on `-I`. A stale copy from an
earlier run is the one that gets compiled, and nothing says so — the build
succeeds and every assertion passes against a header nobody edited. The runner
clears both `home/tmp` and `home/ld` before staging, which is what makes an
edit here actually reach the object.

**A `Long` is not a `long`.** `Zigurat::Long` does not convert and `CAST` will
not force it; `value()` is the accessor. A literal has no members, so
`` 0L.`value() `` is a syntax error and the zero has to be bound first.
