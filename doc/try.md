# TRY CATCH Clause

## Syntax

```ebnf
TRY BEGIN
    [parsi_clause]*
END CATCH variable AS Exception BEGIN
    [parsi_clause]*
END

THROW [Int|String|Exception];

variable ::= [A-Z|a-z|_] [A-Z|a-z|_|0-9]*
parsi_clause ::= Parsi Clauses
```

See also: [Parsi Clauses](parsi.md)

## Example

```parsi
TRY BEGIN
    INSERT INTO human_resources::employees VALUES (1, 'pitarugi');
END CATCH ex AS Exception BEGIN
    IF ex.code() == 1002 BEGIN
        INSERT INTO log_table VALUES (Timestamp::now(), ex.message());
    END ELSE BEGIN
        THROW ex;
    END
END
```
