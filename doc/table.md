# TABLE Clause

## Syntax

```ebnf
TABLE [domain::]* name
[ REQUIRES object_name [, object_name]* ]?
BEGIN
    [COLUMN column_name AS column_type [PRIMARY KEY [DEFAULT expression]? |
        [ [UNIQUE | INDEX] KEY]? [NULL | NOT NULL]? [DEFAULT expression]? ] ;]+
    [ [PRIMARY | UNIQUE | INDEX] KEY (column_name [, column_name]*) ;]*
END

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
object_name ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
column_type ::= Data Types
expression ::= Expression
```

See also: [Data Types](datatypes.md), [Expression](expression.md)

## Example

```parsi
TABLE human_resources::employees
BEGIN
    COLUMN id AS Long PRIMARY KEY;
    COLUMN name AS String UNIQUE KEY NOT NULL;
END
```
