# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Security

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

- JWKS key cache with automatic background refresh (default: 3600 s).
- Rate-limited JWKS fetch to prevent abuse on cache miss.
- 13 new unit tests covering JWKS fetch, cache expiry, signature verification
  failures, algorithm mismatch, and key rotation scenarios.

### Changed

- `OidcAuthProvider` now requires `libcurl` at link time for HTTPS JWKS fetch.
- Build dependency: added `spdlog` for structured logging inside the auth module.
