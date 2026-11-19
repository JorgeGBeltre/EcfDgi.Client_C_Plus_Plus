## Summary
<!-- Explain what this pull request changes and why. -->

## Related issue
<!-- Fixes #123, Closes #123, or References #123. -->

## Change type
- [ ] Bug fix
- [ ] New feature
- [ ] Fiscal/compliance correction
- [ ] Security improvement
- [ ] Performance optimization
- [ ] Refactoring
- [ ] Documentation
- [ ] Tests or build/CI tooling
- [ ] Breaking change

## Affected components
- [ ] Core C++20 fiscal engine (`ecfdgii_core` — `Domain` / `Application` / `Infrastructure`)
- [ ] Drogon REST API & controllers (`ecfdgii_api` — `src/Api`)
- [ ] Canonical XML generation or XSD validation (`libxml2`)
- [ ] XML digital signature or PKCS#12 handling (`xmlsec1` / `OpenSSL`)
- [ ] DGII client, token manager, TLS transport, or background reconciliation (`cpr`)
- [ ] Multi-currency (`<OtraMoneda>`) or monetary calculations
- [ ] Persistence, idempotency, or e-NCF sequence allocation (`libpqxx` / `hiredis`)
- [ ] Security, HMAC authentication, JWT, or multi-tenant isolation
- [ ] CMake, vcpkg, Docker, or CI/CD pipeline
- [ ] Documentation
- [ ] Other: <!-- specify -->

## Technical description
<!-- Describe implementation, affected classes/endpoints, and relevant C++20 design decisions. -->

## Fiscal impact
<!-- Identify affected e-CF types (E31-E47, RFCE, ACECF, ARECF, ANECF), XML fields, DGII rules, rounding, and compatibility.
If not applicable, write "Not applicable". -->

## Security impact
<!-- Explain changes to credentials, PKCS#12 certificates, HKDF/AES-256-GCM decryption, XMLDSig signing, libxml2 parsing,
HMAC/JWT authentication, tenant isolation, memory/resource safety, or logging. If not applicable, say so. -->

## Persistence and concurrency impact
<!-- Describe PostgreSQL schema/queries (libpqxx), Redis cache keys (hiredis), e-NCF sequence locking (FOR UPDATE),
idempotency records, and thread safety across Drogon workers. If not applicable, say so. -->

## Testing
Commands executed and results:

```text
# Exact CMake / CTest commands and output summary
```

- [ ] Regression tests added or updated in `tests/`.
- [ ] New behavior has automated CTest coverage (`validator_tests`, `hmac_tests`, `xsd_tests`, or `parity_tests`).
- [ ] Relevant existing tests pass.
- [ ] Failure and boundary cases considered.
- [ ] Tests were not run (explain why below).

Testing notes:
<!-- Include results, limitations, and compiler/OS environment details. -->

## Compatibility and migration
- [ ] No breaking changes.
- [ ] Breaking changes documented with migration guidance.
- [ ] Public REST API or JSON DTO changes documented.
- [ ] Database schema (`db/schema.sql`) or configuration (`config/appsettings.json`) changes documented.
- [ ] Existing e-CF behavior and multi-environment (`PreCertificacion`, `Certificacion`, `Produccion`) workflows considered.

## Documentation
- [ ] README updated incrementally in English where necessary.
- [ ] Relevant technical documentation updated.
- [ ] Examples and fixtures use synthetic data only (`101000001` / `EMPRESA DEMO SRL`).
- [ ] No documentation update required.

## Pull Request Checklist

### Architecture & C++20 quality
- [ ] Clean Architecture boundaries between `ecfdgii_core` and `ecfdgii_api` are preserved.
- [ ] Native C resources (`libxml2`, `xmlsec1`, `OpenSSL`, `hiredis`) use deterministic RAII cleanup.
- [ ] Changes are focused, free of inline code comments (`//` or `/* */`), and avoid unrelated refactoring.
- [ ] No unnecessary dependencies introduced.

### Fiscal correctness
- [ ] Applicable DGII Technical Format v1.0 rule or XSD reviewed when relevant.
- [ ] Cent-level monetary precision, half-up rounding, and DOP cuadratura preserved and tested when relevant.
- [ ] `<OtraMoneda>` / `<OtraMonedaDetalle>` conversion and XSD element order checked when relevant.
- [ ] Existing e-CF types and regression scenarios considered.

### Security & multi-tenancy
- [ ] No credentials, certificates, private keys, tokens, or real taxpayer data committed.
- [ ] Logs and HTTP error responses do not expose sensitive information.
- [ ] Multi-tenant (`X-Tenant-Id`) and environment (`X-Environment`) isolation preserved.
- [ ] Parameterized SQL queries (`libpqxx`), safe XML parser settings (`XML_PARSE_NONET`), and XMLDSig verification protections were not weakened.

### Final review
- [ ] I reviewed my own diff.
- [ ] Temporary, generated, and unrelated files were removed.
- [ ] I followed the Code of Conduct and contribution guidelines.
