# Expression

An expression is a sequence of operators and their operands, that specifies a computation.
Expression evaluation may produce a result (e.g., evaluation of 2 + 2 produces the result 4) and may generate side-effects (e.g. evaluation of Session::set('id', 4) keeps the integer 4 on the virtual session).
SQL Arithmetic Operators Operator Description + Add - Subtract * Multiply / Divide % Modulo SQL Bitwise Operators Operator Description & Bitwise AND | Bitwise OR ^ Bitwise exclusive OR SQL Comparison Operators Operator Description = Equal to == Equal to > Greater than < Less than >= Greater than or equal to <= Less than or equal to <> Not equal to

## BETWEEN

```ebnf
expression BETWEEN expression AND expression
```

Inclusive at both ends: `x BETWEEN a AND b` is `(x >= a) AND (x <= b)`. It
binds tighter than `AND`, so a following condition is separate. The subject is
evaluated twice, so avoid side effects in it.

```parsi
SELECT id FROM demo::sales WHERE id BETWEEN 1 AND 12;
SELECT id FROM demo::sales WHERE id BETWEEN 1 AND 12 AND region == 'EU';
```

## LIKE

```ebnf
expression LIKE expression
```

Pattern match against a String. `%` matches any run of characters including
none, `_` matches exactly one. NULL on either side yields NULL.

```parsi
SELECT title FROM demo::books WHERE title LIKE 'The %';
SELECT name FROM demo::authors WHERE country LIKE '_S';
```
