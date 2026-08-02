# PROCEDURE Clause

## Syntax

```ebnf
PROCEDURE [domain::]* name ( [ argument [, argument]* ]? )
RETURNS [Void | return_type]
[ REQUIRES object_name [, object_name]* ]?
BEGIN
    [parsi_clause]*
END

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
argument ::= [ argument_name AS argument_type [IN | INOUT | OUT]? [= expression]? ]
argument_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
argument_type ::= Data Types
expression ::= Expression
return_type ::= Data Types
object_name ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
parsi_clause ::= Parsi Clauses
```

See also: [Data Types](datatypes.md), [Expression](expression.md), [Parsi Clauses](parsi.md)

## Example

```parsi
PROCEDURE human_resources::insert_employee(name AS String)
RETURNS Long
REQUIRES human_resources::employees
BEGIN
    DECLARE id AS Long = human_resources::employees_id_sequence::NEXT();
    INSERT INTO human_resources::employees VALUES (id, name);
    RETURN id;
END
```
