# CALL Clause

## Syntax

```ebnf
CALL [domain::]* procedure ( [ parameter [, parameter]* ]? );

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
procedure ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
parameter ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
CALL human_resources::insert_employee('pitarugi');
```
