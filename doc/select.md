# SELECT Clause

## Syntax

```ebnf
SELECT [* | column_name | expression | assignment] [, [* | column_name | expression | assignment] ]*
FROM [domain::]* name
[ WHERE expression [ [AND | OR] expression ]* ]? ;

assignment ::= variable = expression
domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
variable ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md), [SET](set.md)

`SELECT` is a cursor, not a result set. Everything listed between `SELECT` and
`FROM` happens once per row; there is no result object to iterate afterwards.

## Example

```parsi
SELECT id, name
FROM human_resources::employees
WHERE name = 'pitarugi';
```

## Assigning to a variable

An item written `variable = expression` assigns to that variable once per row
instead of being emitted. It is the [SET](set.md) clause, in the one place a
`BEGIN`/`END` block cannot go, and it is how a `SELECT` counts, totals, or keeps
the last value it saw:

```parsi
DECLARE rows AS Long = 0;
DECLARE total AS Long = 0;

SELECT rows = rows + 1, total = total + salary
FROM human_resources::employees
WHERE department == 'sales';

ECHO rows, ' employees, ', total, ' in total';
```

`=` assigns and `==` compares: `SELECT id == 1 FROM t;` emits a `Bool` per row,
while `SELECT last = id FROM t;` writes to `last` and emits nothing. Inside a
`WHERE` clause both spellings mean comparison, as they always have.

Rules worth knowing:

- The variable has to be in scope, so this belongs inside a procedure, function
  or page — declare it with [DECLARE](declare.md) first.
- Assignments run before the other items of the same `SELECT`, in the order they
  were written, so a value one item assigns is the value a later item reads.
- An assignment produces no column: it is not emitted, not sent to a connector
  client, and not named in the cursor's column list. A `SELECT` of nothing but
  assignments returns no rows at all.
- `AS` names a column, so it cannot be applied to an assignment.
