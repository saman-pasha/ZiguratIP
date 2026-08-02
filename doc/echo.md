# ECHO Clause

## Syntax

```ebnf
ECHO expression [, expression]*;

expression ::= Expression
```

See also: [Expression](expression.md)

## Example

```parsi
ECHO '<p>Hello Zigurat</p>';
ECHO '<p>
    Hello Pitarugi
</p>';
ECHO 12, 34.5, TRUE;
```
