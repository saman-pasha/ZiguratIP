# SET Clause

## Syntax

```ebnf
SET [domain::]* variable = expression;

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
variable ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
DECLARE employee_name AS String;
SET employee_name = 'pitarugi';
SET employee_id = human_resources::insert_employee(employee_name);
```
