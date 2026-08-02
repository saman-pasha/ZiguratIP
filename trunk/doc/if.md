# IF ELSE Clause

## Syntax

```ebnf
IF expression [ [AND | OR] expression ]* BEGIN
    [parsi_clause]*
END [ ELSE [ IF expression [ [AND | OR] expression ]* ]? BEGIN
    [parsi_clause]*
END ]*

expression ::= Expression
parsi_clause ::= Parsi Clauses
```

See also: [Expression](expression.md), [Parsi Clauses](parsi.md)

## Example

```parsi
DECLARE var AS Int;
IF other_var == 1 BEGIN
    SET var = 100;
END ELSE BEGIN
    SET var = 101;
END
```
