# UPDATE Clause

## Syntax

```ebnf
UPDATE [domain::]* name
SET column_name = expression [, column_name = expression]*
[ WHERE expression [ [AND | OR] expression ]* ]? ;

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
UPDATE human_resources::employees
SET name = 'pitarugi'
WHERE id = 1;
```
