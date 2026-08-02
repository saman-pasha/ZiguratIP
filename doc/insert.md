# INSERT Clause

## Syntax

```ebnf
INSERT INTO [domain::]* name [ (column_name [, column_name]* ) ]?
VALUES ( expression [, expression]* );

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
INSERT INTO human_resources::employees(id, name)
VALUES (1, 'pitarugi');
INSERT INTO human_resources::employees
VALUES (1, 'pitarugi', human_resources::employee_type::MANAGER);
```
