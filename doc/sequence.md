# SEQUENCE Clause

## Syntax

```ebnf
SEQUENCE [domain::]* name
[REQUIRES object_name [, object_name]* ]?
BEGIN
    FROM expression;
    TO expression;
    STEP expression;
END

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
object_name ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
expression ::= Expression
```

See also: [Expression](expression.md)

## GLOBAL CURRENT() RETURNS Long

Returns a current available value of sequence.

## GLOBAL SET_CURRENT(Long) RETURNS Void

Sets a current available value of sequence.

## GLOBAL NEXT() RETURNS Long

Returns a current available value then sets a current available value add by STEP value.

## GLOBAL BACK() RETURNS Long

Returns a current available value then sets a current available value subtract by STEP value.

## GLOBAL RESET() RETURNS Void

Sets a current available value of sequence to FROM value.

## Example

```parsi
SEQUENCE human_resources::employees_id_sequence
BEGIN
    FROM 1;
    TO Long::MAX;
    STEP 1;
END

DECLARE employee_id AS Long = human_resources::employees_id_sequence::NEXT();
```
