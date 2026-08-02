# TYPE Clause

## Syntax

```ebnf
TYPE [domain::]* name [ <template_argument [, template_argument]> ]?
[ REQUIRES object_name [, object_name]* ]?
AS object_name ;


domain ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
name ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
template_argument ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
object_name ::= [ [A-Z|a-z|_] [A-Z|a-z|_|0-9]*:: ]* [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
```

## Example

```parsi
TYPE hr::emps AS human_resources::employees;
```
