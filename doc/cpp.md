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

## BASE

BASE clause must use after class name for inheriting from external C++ classes.

```ebnf
INCLUDE '<vector>';
CLASS Container::Vector<T> BASE `std::`vector<T>
BEGIN
END
```
