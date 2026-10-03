---
doc_id: DV-CHK-81001
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# IEC 81001-5-1 cybersecurity lifecycle checklist

Candidate basis: IEC 81001-5-1:2021, including Interpretation Sheet 1:2025 and
the corrected December 2025 publication, Clauses 4 through 9 and conditional
Annex F. Consult the adopted interpretation when evaluating supplier software
categories, disclosure, accompanying documentation and maintenance activities.

Assess security across the browser, REST and WebSocket interfaces, DICOM
networking, identity services, storage, deployment and update pipeline. Connect
security failures to safety consequences. Use the
[assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Governance and secure development

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-81001-01 | 4.1 | Assign security responsibilities and competence, determine applicability, review the process and control supplier contributions. | Security plan, role/competence records, supplier assessments and periodic reviews. |
| DV-81001-02 | 4.1 | Establish vulnerability intake, coordinated disclosure and review of security defects and customer guidance. | Published contact/policy, controlled disclosure procedure and review records. |
| DV-81001-03 | 4.2, 7 | Connect threat assessment and security controls to safety risk management. | Threat model, risk criteria and cross-references to the device risk file. |
| DV-81001-04 | 4.3 | Classify maintained, supported and required software and document overlapping responsibilities where applicable. | Classification of viewer code, kcenon libraries, browser, OS, identity provider, DB and deployment services. |
| DV-81001-05 | 5.1 | Protect source, reviews, CI, build runners, signing material and release publication; define secure coding practices. | Access reviews, branch/release controls, secret handling and secure coding rules. |
| DV-81001-06 | 5.2 | Define testable security requirements and evaluate risks from required software before design approval. | Approved requirements, supplier risk assessments and review records. |
| DV-81001-07 | 5.3, 5.4 | Document trust boundaries, attack surfaces, least privilege and defense in depth. Review architecture and detailed interfaces. | Data-flow diagrams, threat scenarios, design reviews and security-control allocation. |
| DV-81001-08 | 5.4 through 5.7 | Verify authentication for REST and WebSocket sessions, token expiry/revocation, JWKS validation and LDAP/OIDC failure behavior. | Positive and negative tests against configured identity providers and reconnect paths. |
| DV-81001-09 | 5.4 through 5.7 | Verify authorization per patient/study, session and operation, including direct API calls and guessed identifiers. | RBAC/object-access tests and isolation results across concurrent users. |
| DV-81001-10 | 5.4 through 5.7 | Verify browser origin, CSRF and cookie controls, WebSocket upgrade checks and protection against script injection. | Browser/API tests, configuration review and penetration findings. |
| DV-81001-11 | 5.4 through 5.7 | Verify TLS and certificate validation on each included connection, including proxy-to-server, DICOMweb, identity and audit transport. | Connection inventory, security settings and failure tests; explicit handling of any unencrypted DIMSE deployment. |
| DV-81001-12 | 5.4 through 5.7 | Validate hostile DICOM, PDU, JSON, project archive and path inputs; bound memory, decompression, concurrency and request size. | Parser fuzzing, malformed-input and resource-exhaustion tests for actual release paths. |
| DV-81001-13 | 5.4 through 5.7 | Protect stored images, project data, credentials, session records and audit events. Define deletion and backup access controls. | Data inventory, encryption/key decisions, access tests and retention/recovery evidence. |
| DV-81001-14 | 5.4 through 5.7 | Preserve audit attribution, ordering/integrity and usable timestamps without leaking unnecessary patient data into logs. | Audit tests, failure handling, clock assumptions and log access/retention policy. |

## Verification and release

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-81001-15 | 5.5, 5.6 | Review implementation and integration against secure coding and interface requirements. | Code review, static analysis, sanitizer and integration results with disposition of findings. |
| DV-81001-16 | 5.7 | Test requirements, threat mitigations, known vulnerabilities, attack surfaces, malformed inputs and penetration scenarios. | Security verification plan and executed reports; a dependency scan alone is insufficient. |
| DV-81001-17 | 5.7 | Establish appropriate independence and competence for security evaluation and review all findings before release. | Evaluator scope/competence, independence rationale, remediation and retest results. |
| DV-81001-18 | 5.8 | Freeze and identify source, dependencies, assets and artifacts; verify delivery integrity and remove development credentials or services. | Release SBOM, dependency hashes, build provenance, artifact integrity checks and configuration audit. |
| DV-81001-19 | 5.8 | Supply installation, hardening, network, account, backup, update and secure-decommissioning instructions. | Reviewed customer security guidance, shared-responsibility model and known limitations. |

## Maintenance and response

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-81001-20 | 6 | Monitor upstream advisories and field reports for every supported release, including pacs_system and transitive libraries. | Component ownership, monitoring cadence, vulnerability register and affected-version analysis. |
| DV-81001-21 | 6 | Assess patches for exploitability, clinical consequences, compatibility and effects on existing controls. | Update impact and prioritization records with safety and security review. |
| DV-81001-22 | 6 | Validate and distribute security updates with rollback, customer communication and post-update monitoring. | Patch verification, deployment/recovery exercise and notification records. |
| DV-81001-23 | 7 | Review threats, controls and residual risks throughout deployment and change, including cloud and on-premise differences. | Updated threat model, residual-risk decisions and control effectiveness evidence. |
| DV-81001-24 | 8 | Preserve security-relevant configuration history and reproduce the exact affected and corrected release. | Controlled configurations, release inventories and traceable change approvals. |
| DV-81001-25 | 9 | Receive, investigate, prioritize, communicate and verify resolution of security problems. Coordinate with clinical incident handling. | Incident/vulnerability cases, risk decisions, disclosure and closure records. |
| DV-81001-26 | 6, 9, Annex F | Define support retirement and assess Annex F eligibility separately. Archive status does not establish a transitional-software exception. | Support/EOL plan; if applicable, Annex F gap assessment, remediation and customer transition evidence. |

## Evidence starting points

- [Authentication](../../src/services/auth), [server API](../../server/src/api), [session validation](../../src/services/render/session_token_validator.cpp).
- [PACS services](../../src/services/pacs), [audit service](../../src/services/pacs/audit_service.cpp), [stores](../../src/services/store), [configuration](../../config).
- [MFDS cybersecurity](mfds-digital-cybersecurity-checklist.md) adds jurisdiction-specific review and reporting decisions.
