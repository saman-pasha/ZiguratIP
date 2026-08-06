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

## Where the two clauses go

Both may be written at file scope or inside a `CLASS`, `PAGE` or `PROCEDURE`.

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

### Limits

* Parsi has no dereference operator and no `*` type suffix, so a block's types
  are reached through free functions and `` `std::`shared_ptr<`T> `` members —
  the same arrangement `System/connector.parsi` uses for `Zigurat::Connector`.
* Names in a block live at global scope, not in the object's namespace. Two
  objects that both declare `struct Net` at global scope collide when both are
  loaded; wrap generated code in a namespace of its own if that is a risk.
* A block is spliced verbatim and is never checked by Parsi. Errors in it are
  C++ compiler errors, reported against the generated file.
