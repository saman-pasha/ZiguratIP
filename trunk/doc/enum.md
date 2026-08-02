# ENUM Clause

## Syntax

```ebnf
ENUM [domain::]* name
BEGIN
    flag_name [, flag_name]*
END

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
flag_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
```

## Example

```parsi
ENUM human_resources::employee_type
BEGIN
    EMPLOYEE,
    MANAGER
END

UPDATE human_resources::employees
SET type = human_resources::employee_type::MANAGER
WHERE id = employee_id;
```
