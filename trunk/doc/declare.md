# DECLARE Clause

## Syntax

```ebnf
DECLARE [ GLOBAL | SESSION LOCAL ]? name AS type [ ( [expression [, expression]* ]? ) | = expression ]? ;

name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
type ::= Data Types
expression ::= Expression
```

See also: [Data Types](datatypes.md), [Expression](expression.md)

## GLOBAL

If LIBRARY/GLOBAL_CACHE_MODE config be TRUE, Variable is defined globally across all sessions and alive during ziguratip process life time.

## SESSION LOCAL

If LIBRARY/GLOBAL_CACHE_MODE or LIBRARY/LOCAL_CACHE_MODE configs be TRUE , Variable is defined locally during session life time.

## Example

```parsi
DECLARE id AS Long;
DECLARE name AS String = 'pitarugi';
DECLARE SESSION LOCAL con AS Connector('localhost', 2160);
```
