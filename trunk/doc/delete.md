# DELETE Clause

## Syntax

```ebnf
DELETE FROM [domain::]* name
[ WHERE expression [ [AND | OR] expression ]* ]? ;

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
DELETE FROM human_resources::employees
WHERE id = 1 OR name = 'pitarugi';
```
