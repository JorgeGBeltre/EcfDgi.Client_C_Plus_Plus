# Contributing to EcfDgi.Client_C_Plus_Plus

Thank you for considering a contribution. EcfDgi.Client_C_Plus_Plus combines a Clean Architecture C++20 fiscal engine (`ecfdgii_core`) with a high-performance Drogon REST API gateway (`ecfdgii_api`) for Dominican Republic electronic invoicing (DGII e-CF). Contributions must preserve fiscal correctness, memory and thread safety, cryptographic security, compatibility, and clean separation between the core fiscal engine and the HTTP API layer.

Please read the [Code of Conduct](CODE_OF_CONDUCT.md). For vulnerabilities, do not open a public issue; follow [SECURITY.md](SECURITY.md).

## 1. Ways to Contribute
You can report reproducible defects, propose enhancements, improve documentation, add CTest unit or parity tests, fix defects, or review pull requests.

## 2. Before Contributing
1. Search existing issues and pull requests.
2. Check current documentation and behavior.
3. Discuss substantial architectural changes before implementation.
4. Describe the problem and expected behavior.
5. Never include production credentials, private keys, certificates, tokens, or identifiable taxpayer data.

## 3. Architecture and Boundaries
The project is organized into Clean Architecture layers across two primary CMake targets:
- **Core fiscal library (`ecfdgii_core` — `src/Domain`, `src/Application`, `src/Infrastructure`, `src/Shared`):** domain entities, interfaces, fiscal validation rules (`EcfValidator`), canonical XML construction (`CanonicalXmlBuilder`), XSD validation (`EcfSchemaValidator` via `libxml2`), XMLDSig RSA-SHA256 signing (`EcfXmlSigner` via `xmlsec1` and `OpenSSL`), multi-tenant PKCS#12 certificate resolution (`TenantSignerResolver`), DGII HTTP/TLS transport (`DgiiDirectTransport` and `EcfTokenManager` via `cpr`/`libcurl`), background status reconciliation (`EcfStatusReconciler`), and PostgreSQL (`libpqxx`) / Redis (`hiredis`) persistence.
- **Drogon REST API layer (`ecfdgii_api` — `src/Api`):** HTTP controllers (`DocumentsController`, `EcfController`, `EmisorReceptorController`, `CustomersController`, `AuthController`), authentication and authorization filters (`UserOrWorkerFilter`, `OptionalUserOrWorkerFilter`, `JwtAuthFilter`, `AdminRoleFilter`), idempotency handling (`IdempotencyHandler`), replay protection (`NonceCache`), and JSON mapping (`JsonMapping`).

Rules:
- Do not introduce Drogon HTTP dependencies into `Domain` or `Application`.
- Keep canonical XML construction, monetary conversion, XSD validation, and XMLDSig signing inside the core fiscal engine services rather than duplicating them across controllers.
- Reuse existing domain and application services instead of duplicating fiscal logic.
- Avoid circular dependencies and unnecessary abstractions.
- Explain any new vcpkg or runtime dependency.
- Preserve public REST API contracts and C++ interfaces unless a breaking change is justified and documented.

Follow the source layout currently in the repository; do not create parallel structures without a clear reason.

## 4. Development Environment
Inspect the repository's current build configuration (`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `triplets/`, and `Dockerfile`). Use a C++20-compliant compiler (`GCC 13+`, `Clang 16+`, or `MSVC 2022`), `CMake 3.24+`, `Ninja`, and `vcpkg` in manifest mode. Do not assume a local tool or system path exists across environments.

## 5. Coding Standards
- Follow C++20 conventions and namespaces (`ecf::domain`, `ecf::app`, `ecf::infra`, `ecf::api`, `ecf::sys`) in the affected module.
- Use strict RAII and deterministic cleanup for native C resources (`xmlDocPtr`, `xmlSchemaPtr`, `xmlSecDSigCtxPtr`, `EVP_PKEY*`, `X509*`, `BIO*`, `redisContext*`).
- Protect shared state across Drogon event-loop threads with appropriate synchronization (`std::mutex`, atomic operations, or thread-safe connection pools).
- Handle errors explicitly using `Result<T>` or domain exceptions (`EcfException`, `ValidationException`); do not silently swallow failures.
- Never log secrets, PKCS#12 certificate bytes, certificate passwords, HMAC keys, JWT secrets, tokens, or sensitive fiscal payloads.
- Keep source and test files clean and self-documenting without inline code comments (`//` or `/* ... */`).
- Do not commit build directories (`build/`, `vcpkg_installed/`), local databases, `.env` files, real certificates (`.p12`, `.pfx`, `.pem`), or editor-specific artifacts.

## 6. Fiscal and Monetary Correctness
Changes involving e-CF types (`E31`–`E47`), RFCE (`E32 < RD$250,000.00`), XML fields, DGII validations, ITBIS taxes, discounts, retentions, credit/debit notes (`E33`/`E34`), commercial approvals (`ACECF`), acknowledgements (`ARECF`), or sequence annulments (`ANECF`) require particular care:
- Identify the affected document type and XML section.
- Reference applicable official DGII Technical Format v1.0 rules or XSDs when available.
- Distinguish specification requirements from project-level decisions.
- Add valid, invalid, and boundary tests in `tests/` as appropriate.
- Preserve unaffected document behavior.
- Use synthetic demo test data only (e.g., `101000001` / `EMPRESA DEMO SRL`).

### Monetary precision and multi-currency (`<OtraMoneda>`)
- Preserve cent-level precision and half-up rounding (`round2` / `round4`, `fmt2`) in all monetary calculations.
- Ensure line-level and header-level DOP cuadratura (`CantidadItem * PrecioUnitarioItem - DescuentoMonto == MontoItem`) when converting foreign currencies (`USD`, `EUR`, and other official DGII `TipoMonedaType` codes) to DOP.
- Emit `<OtraMonedaDetalle>` and `<OtraMoneda>` in the exact XSD element sequence required by each `TipoeCF` (`31`–`47`).
- Do not change currency codes, precision, or rounding rules based on assumptions alone.

### XML and signatures
- Test XML generation, parsing, XSD validation, canonicalization, and XMLDSig RSA-SHA256 signing changes.
- Validate against the applicable schema in `Documentación Técnica (XSD)/`.
- Preserve required element ordering and whitespace policy (`preserveWhiteSpace` parity between signing and verification).
- Treat external XML as untrusted input and preserve safe `libxml2` parser flags (such as `XML_PARSE_NONET`) to prevent external entity or network resolution.
- Do not weaken signature verification or XSD validation to make a test pass.
- Use ephemeral or synthetic test certificates; never commit production certificates or private keys.

Do not claim DGII certification or official approval without verifiable evidence.

## 7. API, Persistence, and Multi-Tenant Isolation
- Preserve strict tenant and environment isolation (`X-Tenant-Id`, `X-Environment`, `?environment=`).
- Use parameterized `libpqxx` queries for all PostgreSQL operations; never concatenate untrusted strings into SQL.
- Preserve row-level transactional locking (`SELECT ... FOR UPDATE` in `EcfSequenceManager`) when modifying e-NCF sequence allocation or unused sequence rollback.
- Preserve idempotency guarantees (`DbIdempotencyStore`) and HMAC-SHA256 worker request canonicalization and replay protection (`CanonicalRequestHelper`, `NonceCache`).

## 8. Tests and Quality Checks
Build and run the CTest test suite before submitting changes:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Ensure all test executables (`validator_tests`, `hmac_tests`, `xsd_tests`, and `parity_tests`) pass. Add regression tests for bug fixes, tests for new behavior, and failure/boundary tests for fiscal or security-sensitive changes. In the PR, list exact commands and results. Never claim tests passed if they were not run.

## 9. Pull Request Process
1. Create a focused branch from `develop` (or the expected base branch).
2. Make the smallest coherent change that solves the problem.
3. Add or update tests and documentation.
4. Review the diff for secrets, real taxpayer data, comments, generated files, unrelated changes, and accidental API changes.
5. Open a pull request using the repository template.
6. Respond to review feedback with technical explanations and follow-up commits.

Explain the problem, implementation, affected components, tests and results, and fiscal/security/compatibility/migration impact as applicable.

## 10. Commit Messages
Clear descriptive messages in English using Conventional Commits are required:

```text
type(scope): concise description
```

Examples: `fix(xml): enforce XSD element ordering for E43 OtraMoneda`, `perf(http): reuse persistent cpr::Session keep-alive TLS connections`, `test(parity): cover E34 Rule 615 pre-allocation amount cap`.

Do not add co-author trailers or attribution for people or tools that did not author the commit. Respect the repository's configured identity and contribution policy.

## 11. Breaking Changes and Releases
Do not introduce breaking changes silently. Explain affected REST endpoints, JSON DTOs, C++ interfaces, database schema (`db/schema.sql`), and configuration (`config/appsettings.json`); provide migration guidance; update tests and docs; and flag the change in the PR. Release decisions are made by maintainers.

## 12. Documentation
Update documentation incrementally in English when installation, build presets, REST endpoints, configuration, e-CF behavior, currency/rounding, or security controls change. Use synthetic examples only.

## 13. Questions
Use Discussions if enabled, or open an issue with sufficient context. For vulnerabilities, follow [SECURITY.md](SECURITY.md).
