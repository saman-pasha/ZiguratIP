# CLASS Clause

## Syntax

```ebnf
CLASS [domain::]* name [ <template_argument [, template_argument]> ]?
[ INHERITS object_name [, object_name]* ]?
[ REQUIRES object_name [, object_name]* ]?
BEGIN
[
[ PRIVATE | PROTECTED | PUBLIC ]:
    [
    [USING object_name;] |
    [declare_clause] |
    [CONSTRUCTOR([ argument [, argument]* ]? ) BEGIN
        [parsi_clause]*
    END] |
    [GLOBAL | [ [[VIRTUAL]? [OVERRIDE]?] | [PURE VIRTUAL]? ]]?
    [FUNCTION function_name [ <template_argument [, template_argument]> ]?
    ([ argument [, argument]* ]? ) BEGIN
        [parsi_clause]*
    END] |
    [DESTRUCTOR() BEGIN
        [parsi_clause]*
    END]
    ]*
]*
END

domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
template_argument ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
object_name ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
argument ::= [ argument_name AS argument_type [IN | INOUT | OUT]? [= expression]? ]
argument_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
argument_type ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
function_name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
declare_clause ::= Declare Clause
parsi_clause ::= Parsi Language
expression ::= Expression
```

See also: [Declare Clause](declare.md), [Parsi Language](parsi.md), [Expression](expression.md)

## Example

```parsi
CLASS Math
BEGIN
PUBLIC:
    GLOBAL FUNCTION pow<T>(base AS T, expo AS T) RETURNS T
    BEGIN
        RETURN `std::`pow(base, expo);
    END
END
```
