# SELECT Clause

## Syntax

```ebnf
SELECT [* | column_name | expression] [, [* | column_name | expression] ]*
FROM [domain::]* name
[ WHERE expression [ [AND | OR] expression ]* ]? ;

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
SELECT id, name
FROM human_resources::employees
WHERE name = 'pitarugi';
```
