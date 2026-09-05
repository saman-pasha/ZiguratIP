# TABLE Clause

## Syntax

```ebnf
TABLE [domain::]* name
[ REQUIRES object_name [, object_name]* ]?
BEGIN
    [COLUMN column_name AS column_type [PRIMARY KEY [DEFAULT expression]? |
        [ [UNIQUE | INDEX] KEY]? [NULL | NOT NULL]? [DEFAULT expression]? ] ;]+
    [ [PRIMARY | UNIQUE | INDEX] KEY (column_name [, column_name]*) ;]*
END

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
object_name ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_type ::= Data Types
expression ::= Expression
```

See also: [Data Types](datatypes.md), [Expression](expression.md)

## Keys

Every data type is a key. The engine's B-tree holds one int64 per key, and
each type folds to one:

* the integral family (`Bool`, `Char`, `Byte`, `UByte`, `Short`, `UShort`,
  `Int`, `UInt`, `Long`, `ULong`, `Timestamp`) as itself -- a `ULong` above
  2^63 keeps equality but not order;
* `Float`, `Double` and `Real` through an order-preserving fold of the double,
  so `<`, `<=`, `>`, `>=` over such a key are real ranges;
* `String` and `Text` as a 64-bit hash of the bytes, and a `Vector` as a hash
  over its elements' folds. A hash orders nothing, so **only equality reaches
  a hashed index**: a range or `<>` over a `String`, `Text` or `Vector` key
  takes the table scan instead, filtered by the full predicate. A hash can
  also collide; every indexed lookup re-applies its whole `WHERE` to each row
  the index hands back, so a collision costs a row visit and never a wrong
  answer. The one place it shows is a `UNIQUE KEY` over a hashed type, which
  would refuse a distinct value whose hash another row already carries -- at
  2^63 odds, and loudly.

A composite key folds each column by its own type; the same rule applies per
level.

## Example

```parsi
TABLE human_resources::employees
BEGIN
    COLUMN id AS Long PRIMARY KEY;
    COLUMN name AS String UNIQUE KEY NOT NULL;
END
```
