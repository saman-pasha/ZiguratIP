# DO WHILE Clause

## Syntax

```ebnf
DO BEGIN
    [parsi_clause]*
END WHILE expression [ [AND | OR] expression ]* ;

WHILE expression [ [AND | OR] expression ]* BEGIN
    [parsi_clause]*
END

expression ::= Expression
parsi_clause ::= Parsi Clauses
```

See also: [Expression](expression.md), [Parsi Clauses](parsi.md)

## Example

```parsi
DECLARE var AS Int = 0;
DO BEGIN
    SET var = var + 1;
END WHILE var < 10;
WHILE var < 20 BEGIN
    SET var = var + 1;
END
```
