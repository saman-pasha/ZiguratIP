# Certificates and keys

This is where ZiguratIP looks for certificate material: the issuer
configuration the `ca` tool defaults to, its key pair, and the certificates a
secure connection will be built on. `SECURITY/CERTIFICATE_PATH` in
`ziguratip.conf` moves the whole directory somewhere else — off the install
prefix and out of any repository, which is where real keys belong.

## Sample CA material — do not use

Everything named `dont-use-*` in this directory is a **sample**, committed so
the `ca` examples run against a working default without anyone having to
generate a key first.

- `dont-use-private.key` — an unencrypted RSA private key
- `dont-use-public.key` — its public half, a DER `SubjectPublicKeyInfo`
- `dont-use-certificate.crt` — a self-signed certificate for the key, DER

All three read cleanly in OpenSSL, so a certificate issued against these
defaults can be checked with something other than `ca`:

```bash
openssl x509 -in dont-use-certificate.crt -inform DER -out ca.pem
openssl verify -CAfile ca.pem your-certificate.pem
```

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
