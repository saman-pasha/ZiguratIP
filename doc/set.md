# SET Clause

## Syntax

```ebnf
SET [domain::]* variable = expression;

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
variable ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md), [SELECT](select.md)

## Example

```parsi
DECLARE employee_name AS String;
SET employee_name = 'pitarugi';
SET employee_id = human_resources::insert_employee(employee_name);
```

## Inside a SELECT

`SET` is a clause, so it needs a statement block to live in. A `SELECT` has no
block — it is one statement whose items happen once per row — so it takes the
assignment directly, without the keyword:

```parsi
SELECT total = total + salary FROM human_resources::employees;
```

See [SELECT](select.md#assigning-to-a-variable).
