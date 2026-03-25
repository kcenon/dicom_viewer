# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Security

- Migrate JWT access token storage from localStorage to httpOnly cookies (#547).
  Login, refresh, and logout endpoints now set `access_token` as an
  `HttpOnly; Secure; SameSite=Strict` cookie, eliminating XSS token theft.
  The client no longer stores the JWT in localStorage or sessionStorage.
  A CSRF double-submit cookie pattern (`csrf_token` cookie + `X-CSRF-Token`
  header) protects state-changing requests (POST/PUT/DELETE).
- Remove unauthenticated fallback when JWT validator is null (#546).
  `JwtMiddleware` now returns HTTP 500 (fail-closed) instead of granting
  anonymous access when the validator pointer is not set. A startup guard
  in `main.cpp` prevents the server from launching without a valid
  JWT validator.
- Implement OIDC JWKS signature verification in `OidcAuthProvider::validateToken()` (#545).
  Previously only performed structural JWT validation (decode + expiry check);
  now fetches the provider's JWKS endpoint, caches keys with configurable
  refresh interval (`jwksRefreshIntervalSeconds`), and cryptographically
  verifies RS256 signatures before accepting tokens.

### Added

- `GET /api/v1/auth/csrf-token` endpoint for obtaining a fresh CSRF token pair.
- `GET /api/v1/auth/me` endpoint for cookie-based session verification on
  page load.
- CSRF double-submit cookie validation in `JwtMiddleware` for POST/PUT/DELETE
  requests when the JWT is sourced from a cookie.
- 8 new unit tests covering cookie token extraction, cookie-over-Bearer
  priority, CSRF validation for state-changing methods, CSRF bypass for
  GET requests and Bearer auth.
- JWKS key cache with automatic background refresh (default: 3600 s).
- Rate-limited JWKS fetch to prevent abuse on cache miss.
- 13 new unit tests covering JWKS fetch, cache expiry, signature verification
  failures, algorithm mismatch, and key rotation scenarios.

### Changed

- `JwtMiddleware` resolves tokens from Cookie header first, falling back to
  `Authorization: Bearer` for non-browser API clients.
- Login response no longer includes `accessToken` in the JSON body; the token
  is delivered exclusively via `Set-Cookie`.
- CORS headers now include `X-CSRF-Token` in `Access-Control-Allow-Headers`
  and set `Access-Control-Allow-Credentials: true`.
- Client `httpClient` sends `credentials: 'include'` on all fetch requests
  and attaches `X-CSRF-Token` header for state-changing methods.
- `OidcAuthProvider` now requires `libcurl` at link time for HTTPS JWKS fetch.
- Build dependency: added `spdlog` for structured logging inside the auth module.
- CI test steps no longer use `continue-on-error`; test failures now block
  PR merges (#548). The `publish-unit-test-result-action` `fail_on` setting
  is restored to `"test failures"`.

### Fixed

- Fix DICOM echo latency assertion that failed on fast localhost connections
  where round-trip time is sub-millisecond (`EXPECT_GT` -> `EXPECT_GE`) (#548).
- Skip PDF generation tests (`ReportGeneratorTest`) when `wkhtmltopdf` is not
  installed, preventing hard failures on CI runners (#548).
- Fix JWT token tampering test that was a no-op due to base64url padding
  alignment; tamper now targets the middle of the signature portion (#548).
- Fix `find_library` calls for `openjp2`, `openjph`, and `charls` in
  `CMakeLists.txt` to search `PACS_SYSTEM_LIB_DIR` first (#548).
