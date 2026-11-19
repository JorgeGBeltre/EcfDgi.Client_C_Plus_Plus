# Security Policy

## Supported Versions
Security fixes are prioritized for the latest maintained release on `main`. Support for older releases is not guaranteed; upgrade to the latest maintained release unless maintainers explicitly announce otherwise.

| Version | Security support |
|---|---|
| Latest maintained release (`2.x` / `main`) | Security fixes prioritized |
| Older releases | Not guaranteed |

Repository release notes and announcements are the source of truth for current maintenance status.

## Reporting a Vulnerability
**Do not report vulnerabilities through public issues, discussions, pull requests, or comments.**

Use GitHub **Private Vulnerability Reporting** for this repository ([Report a vulnerability](https://github.com/JorgeGBeltre/EcfDgi.Client_C_Plus_Plus/security/advisories/new)), if enabled, or create a private GitHub Security Advisory and request maintainer collaboration. If private reporting is unavailable, contact a maintainer through a private channel listed by the repository. Do not send sensitive details through public channels.

Include, where safely possible:
- Vulnerability description and affected version/commit/component.
- Preconditions, build/runtime environment, and configuration.
- Reproduction steps or a minimal proof of concept using synthetic data.
- Expected and actual security impact.
- Relevant logs with secrets and personal data removed.
- Any known workaround or mitigation.

Allow maintainers a reasonable opportunity to investigate and prepare a fix before public disclosure. Do not access, modify, or retain third-party data during research.

## Sensitive Data: Never Publish
Never attach or commit production PKCS#12/P12/PFX/PEM certificates or private keys, certificate passwords, encryption master keys (`STORAGE_ENCRYPTION_KEY`, `CERT_ENCRYPTION_KEY`, `JWT_SECRET`, `WORKER_SECRET_KEY`), DGII credentials, API keys, tokens, production `.env` files or database dumps, real XML containing identifiable fiscal or personal data, or unredacted logs and backups. Use synthetic data (`101000001` / `EMPRESA DEMO SRL`), ephemerally generated test certificates, and redacted logs.

## Security-Critical Areas
Reports are especially relevant to:
- PKCS#12 certificate loading, password handling, in-memory signer caching, and X.509 subject/RNC ownership validation (`EcfXmlSigner`, `TenantSignerResolver`).
- Encryption and key derivation, including `enc:v3:` / `enc:v2:` HKDF-SHA256 and AES-256-GCM implementations (`OpenSSL` / `libsodium`).
- XML digital signature (XMLDSig RSA-SHA256) generation and verification (`xmlsec1`).
- XML parsing, canonicalization, XSD schema validation (`libxml2`), XXE prevention, and `XML_PARSE_NONET` external resource restrictions.
- DGII authentication (`/semilla` and `/validarsemilla`), token caching (`EcfTokenManager`), HTTPS/TLS transport (`cpr` / `libcurl`), and certificate verification.
- HMAC-SHA256 worker authentication, canonical request signing (`CanonicalRequestHelper`), timestamp skew validation, and replay protection (`NonceCache`).
- JWT authentication, Argon2id password hashing (`libsodium`), and Drogon authorization filters (`UserOrWorkerFilter`, `JwtAuthFilter`, `AdminRoleFilter`).
- Multi-tenant and multi-environment (`PreCertificacion`, `Certificacion`, `Produccion`) data and certificate isolation.
- PostgreSQL query parameterization (`libpqxx`), sequence concurrency locking (`EcfSequenceManager`), and idempotency handling (`DbIdempotencyStore`).
- Native C++20 memory safety, RAII resource lifecycle, integer/buffer bounds, and thread safety across Drogon workers.
- Sensitive data exposure through logs, exceptions, HTTP error payloads, or generated files.

This list is not exhaustive and does not assert that every control is present in every release. Report observed behavior and affected versions.

## What to Expect
Maintainers will make reasonable efforts to acknowledge reports privately, assess severity and reproducibility, coordinate investigation, prepare and test a fix or mitigation, and publish an advisory and corrected release when appropriate. Response times depend on severity, complexity, dependencies, and maintainer availability. This policy makes no fixed response-time guarantee.

## Coordinated Disclosure
Do not publicly disclose an unpatched vulnerability or exploit details before coordinating with maintainers. Once a fix or mitigation is available, maintainers may publish an advisory describing impact, affected versions, remediation, and upgrade steps.

## Scope and Good-Faith Research
This policy covers this repository and its maintained release and container artifacts. Vulnerabilities in DGII infrastructure, Drogon, OpenSSL, libxml2, xmlsec1, libpqxx, or other third-party dependencies should be reported to their respective maintainers; privately explain any project-specific impact to this project's maintainers.

Testing must be authorized, non-destructive, and limited to systems and data you own or are explicitly permitted to test.
