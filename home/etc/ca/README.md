# Sample CA material — do not use

Everything named `dont-use-*` in this directory is a **sample**, committed so
the `ca` examples run against a working default without anyone having to
generate a key first.

- `dont-use-private.key` — an unencrypted RSA private key
- `dont-use-public.key` — its public half
- `dont-use-certificate.crt` — an empty placeholder

The private key is public: it is in this repository, and in every clone and
every fork of it. Anything signed with it can be forged by anyone. It is not a
secret and was never meant to be one.

## For anything real

Generate your own, and keep it out of the repository — `*.key` is ignored by
default precisely so this cannot happen by accident:

```bash
ca keygen --signature=RSA-2048 --encryption=AES-256 --cipher="your passphrase" \
          --private=issuer.key --public=issuer.pub
```

Then point the tool at it explicitly:

```bash
ca issue --issuer=issuer.conf --issuer-pik=issuer.key --cipher="your passphrase" \
         --csr=request.csr --certificate=certificate.crt
```

`issuer.conf` holds the distinguished name the certificates are issued under;
edit it before issuing anything.
