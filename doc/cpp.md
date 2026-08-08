# C++ Helpers

Parsi has some clauses to help developers to connect other C++ libraries into their code.
Due to Parsi is case insensitive, C++ imported classes should be used case sensitive. Parsi uses backquot '`' character befor every names to make it case sensitive. like '`std::`mutex'.

## INCLUDE

INCLUDE clause includes C++ headers into your code.

```ebnf
INCLUDE '"header.hpp" | <header>';
```

## LINK

LINK clause links C++ library at build time.

```ebnf
LINK '-llibrary|-Llibrary_full_path';
```

## COMPILE

COMPILE clause adds a flag to the C++ **compile** of this object.

```ebnf
COMPILE '-std=c++17|-Iinclude_path|-Dmacro';
```

`LINK` and `COMPILE` are not interchangeable. `LINK` reaches the linker, and a
`-I` written there lands after the object file where it does nothing; `COMPILE`
reaches the compiler, which is where a header search path and a language
standard have to be.

An object's flags are appended after the configured `COMPILER/CPP_FLAGS`, so
`COMPILE '-std=c++17';` wins over a `-std=c++11` in the configuration — the last
`-std` on a `g++` or `clang++` line is the one that applies. libtorch 2.x needs
exactly that, and it is what `Test/ai/classifier.parsi` is compiled with:

```
CLASS Demo::Classifier
BEGIN
    COMPILE '-std=c++17';
    COMPILE '-I/opt/torch/include';
    COMPILE '-I/opt/torch/include/torch/csrc/api/include';
    LINK '-L/opt/torch/lib';
    LINK '-ltorch';
    ...
END
```

Before this clause existed the only way to reach a library's headers was to edit
`COMPILER/CPP_FLAGS` in the server's `home/etc/ziguratip.conf` and put it back
afterwards — a global setting changed for one object, wrong for everything else
compiled while it was in place.

## Where the three clauses go

All three may be written at file scope or inside a `CLASS`, `PAGE` or `PROCEDURE`.

At file scope they accumulate: a clause applies to every object declared after
it in the same file. Inside an object it belongs to that object alone, and the
next object in the file does not inherit it. Since every object compiles
standalone into its own `.so`, that is where a dependency usually belongs —
one class's `-ltorch` is not its neighbour's.

```
INCLUDE '<vector>';        -- both classes get this

CLASS Model
BEGIN
    INCLUDE '<torch/torch.h>';   -- only Model
    LINK '-ltorch';
    ...
END

CLASS Report                     -- no torch here
BEGIN
    ...
END
```

An object-level `INCLUDE` lands in that object's generated header, so every
object that `REQUIRES` it inherits the header too. To keep a heavy header out of
the objects downstream, put it in a `BEGIN CPP` block instead — see below.
`COMPILE` has no such reach: a flag compiles this object and nothing else, so
the `-I` that finds a header stays with the object that needs it.

## BASE

BASE clause must use after class name for inheriting from external C++ classes.

```ebnf
INCLUDE '<vector>';
CLASS Container::Vector<T> BASE `std::`vector<T>
BEGIN
END
```

## BEGIN HPP and BEGIN CPP

A block of C++ taken whole. `BEGIN HPP` goes into the object's generated header,
`BEGIN CPP` into its implementation, both **above the namespace** so a block can
bring its own `#include` with it. Available inside `CLASS`, `PAGE` and
`PROCEDURE`.

```ebnf
BEGIN HPP
  ... C++ ...
END

BEGIN CPP
  ... C++ ...
END
```

The text is not tokenized. Nothing inside a block is interpreted as Parsi: `<`
is not an operator, `//` does not open a comment, `::` does not separate names,
and nothing is upper-cased. **A block ends at a line that is nothing but `END`**
— C++ may say `END` wherever it likes as long as it is never alone on a line.

### What the two are for

Declare in `HPP`, define in `CPP`, and the class holds a handle:

```
CLASS demo::classifier
BEGIN
	LINK '-ltorch';

	BEGIN HPP
	#include <memory>
	struct Net;
	std::shared_ptr<Net> net_load(const char* path);
	long net_predict(const std::shared_ptr<Net>&, const float* pixels);
	END

	BEGIN CPP
	#include <torch/torch.h>
	struct Net : torch::nn::Module { ... };
	std::shared_ptr<Net> net_load(const char* path) { ... }
	long net_predict(const std::shared_ptr<Net>& n, const float* p) { ... }
	END

PRIVATE:
	DECLARE _net AS `std::`shared_ptr<`Net>;
PUBLIC:
	FUNCTION predict(...) RETURNS Long
	BEGIN
		RETURN `net_predict(this._net, ...);
	END
END
```

The split is the point. Every object that `REQUIRES demo::classifier` includes
its header, so anything in `HPP` is inherited by all of them — while `CPP` stays
private to this object's own translation unit. Put the forward declaration in
`HPP` and `<torch/torch.h>` in `CPP`, and a page calling the model **inherits
the linkage without paying for the header**.

Linkage is inherited either way: `LINK` flags travel through `REQUIRES` (each
object re-exports what it linked), so only the object that owns the library ever
names it.

### Passing a value in

A block's functions take plain C++ types, and a Parsi `Long` is
`Zigurat::Long`, not `long`. It does not convert and `CAST` will not force it;
`value()` is the accessor:

```
FUNCTION bump(by AS Long) RETURNS Long
BEGIN
	RETURN `net_bump(this._net, by.`value());
END
```

A literal has no members, so `0L.`value()` is a syntax error — bind it first
with `DECLARE zero AS Long = 0;`.

### Generated blocks

Nothing about a block cares where the C++ came from, and a transpiler that can
write a fragment can fill one. [Cicili](https://github.com/saman-pasha/cicili)
does: a `header` target gives the declarations and a `source` target with no
`:compile` gives the definitions, neither carrying an `#include` of its own —
see `example/parsi-fragment.cicili` there for the whole path, this end included.

### Limits

* Parsi has no dereference operator and no `*` type suffix, so a block's types
  are reached through free functions and `` `std::`shared_ptr<`T> `` members —
  the same arrangement `System/connector.parsi` uses for `Zigurat::Connector`.
* Names in a block live at global scope, not in the object's namespace. Two
  objects that both declare `struct Net` at global scope collide when both are
  loaded; wrap generated code in a namespace of its own if that is a risk.
* A block is spliced verbatim and is never checked by Parsi. Errors in it are
  C++ compiler errors, reported against the generated file.
* **A `CPP` block carries its own headers.** It is emitted above
  `#include "_NAME_.hpp"`, so an object-level `INCLUDE` — which lands in the
  header — is not in scope inside it. Repeat what the block needs.

  The order is not arbitrary. `globals.hpp` defines `THIS`, `CAST`, `AUTO`,
  `VOID`, `TRUE`, `FALSE` and `NULL` as bare macros, and a C++ library is
  entitled to use any of those as an identifier — libtorch has

  ```cpp
  enum class CuDNNDepthwiseKernel { AUTO, CUDNN, NATIVE };
  ```

  which with `AUTO` defined reads as `enum class { auto, ... }` and takes the
  rest of the header down with it. Emitted after the header include, a block
  could not include libtorch at all.
