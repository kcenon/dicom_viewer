---
doc_id: DV-CHK-INDEX
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
baseline_commit: 172b4ec901a4df7dfbff82045832d249ab6ddd08
---

# SaMD certification readiness checklists

Use these checklists to plan and record the evidence needed to assess
`dicom_viewer` as Software as a Medical Device (SaMD). They cover software
development, product validation, risk, usability, cybersecurity, organizational
quality controls, and a potential South Korean MFDS submission.

No medical device classification, software safety class, market authorization,
QMS certification, or conformity finding is established by these documents.
The intended use and release scope must be approved before applicability can be
decided. MFDS requirements are conditional on the selected market and product
classification. The IEC and ISO documents are candidate standards to be adopted
through a recorded decision.

## Product boundary

The baseline implementation has a React and TypeScript browser client, a C++23
server with Crow REST and WebSocket interfaces, ITK and GDCM image processing,
VTK rendering, and `pacs_system` connectivity. Review the complete deployed
product: client assets, server executable, configuration, identity provider,
storage, reverse proxy, GPU, operating system, browser, and hospital interfaces.
Identify which of these are supplied, required, or operated by another party.

Candidate functions include CT/MRI viewing, MPR, segmentation, measurements,
4D Flow MRI, cardiac analysis, DICOM SR and other exports, and PACS exchange.
Their presence in source code or the README does not establish an approved
clinical claim. Record each function as included, excluded, or undecided for the
release. Deterministic image processing does not by itself establish an AI/ML
claim; assess any model or AI service actually included in the product.

## Checklist index

| Checklist | Review scope | Related reviews |
|---|---|---|
| [IEC 62304](iec-62304-checklist.md) | Software lifecycle, Clauses 4 through 9 | Risk, security, MFDS GMP software activities |
| [ISO 13485](iso-13485-checklist.md) | Manufacturer QMS, Clauses 4 through 8 | MFDS GMP and external organizational records |
| [ISO 14971](iso-14971-checklist.md) | Risk management, Clauses 4 through 10 | Software, usability, security, clinical claims |
| [IEC 62366-1](iec-62366-1-checklist.md) | Usability engineering, Clauses 4 and 5; conditional Annex C | Risk and product validation |
| [IEC 81001-5-1](iec-81001-5-1-checklist.md) | Security lifecycle, Clauses 4 through 9; conditional Annex F | Software lifecycle and MFDS cybersecurity |
| [IEC 82304-1](iec-82304-1-checklist.md) | Product requirements, validation, documentation and maintenance, Clauses 4 through 8 | Software lifecycle and clinical evidence |
| [MFDS digital device approval](mfds-digital-device-approval-checklist.md) | Classification, application evidence, labeling and changes | All applicable technical reviews |
| [MFDS digital GMP](mfds-digital-gmp-checklist.md) | QMS, software and conditional AI controls in Annexes 2 through 4 | ISO 13485 and IEC 62304 |
| [MFDS digital cybersecurity](mfds-digital-cybersecurity-checklist.md) | Manufacturer security activities and submission evidence | IEC 81001-5-1 and approval dossier |

These are project-specific review prompts grouped by clause or activity, not an
exhaustive transcription of every normative subclause. Before using a completed
review for submission, reconcile its coverage with the controlled editions and
record additional items needed for the selected product and market. Sources,
editions, adaptation scope and regulatory update checks are in the
[reference register](regulatory-references.md).

## Decisions required before assessment

All decisions below are **undecided in this checklist package**. Existing PRD
personas and feature descriptions are inputs for review, not approved decisions.

| ID | Decision to record | Suggested accountable role |
|---|---|---|
| DV-DEC-01 | Legal manufacturer, product name, model, release identifier, release authority and support organization | Management and quality |
| DV-DEC-02 | Intended medical purpose, indications, patient population, intended users, care setting, contraindications and exclusions | Product, clinical and regulatory |
| DV-DEC-03 | Target markets, device qualification, product code, regulatory class and authorization route | Regulatory |
| DV-DEC-04 | IEC 62304 safety class for the system and any separately classified items, with segregation rationale | Risk and software leads |
| DV-DEC-05 | Included viewing, diagnostic, measurement, flow, cardiac, export and PACS functions; disabled or research-only functions | Product and clinical |
| DV-DEC-06 | Supported server OS, GPU, browser, display, network, identity, database and deployment configurations | Software and operations |
| DV-DEC-07 | Adopted standards, editions, amendments, interpretations, national versions and documented exclusions | Regulatory and quality |
| DV-DEC-08 | AI/ML presence, model ownership, learning behavior and applicability of additional controls | Product, software and regulatory |
| DV-DEC-09 | Support period, supplier monitoring, update commitments, complaint intake and end-of-support process | Management and operations |

## How to record a review

1. Copy the [review record template](review-record-template.md) into a controlled
   review location. Record the exact product SHA, dependency set, artifacts,
   configuration, checklist revision, adopted source editions and actual date.
2. Create one assessment row for every selected checklist ID. Record omitted
   IDs as `Not reviewed`; never infer completion from the presence of code.
3. Keep applicability, finding and evidence availability separate. A document
   request still awaiting a supplier or the quality organization is `Pending
   evidence`, not proof that the process does not exist.
4. A `Satisfied` finding requires reviewed evidence for the stated build and
   scope. For a grouped prompt, assess every part; split it into child rows if
   findings differ. A parent cannot be satisfied while an applicable child is
   open. Non-applicability needs a rationale and authorized review.
5. Link open findings to an owner, action and due date. Link risk controls to
   requirements, design, implementation and verification results. Preserve
   external record identifiers and revisions when evidence is held in the QMS.
6. Keep patient data, credentials, paid standards and confidential QMS records
   in their approved repositories. Public review records should use controlled
   identifiers or de-identified evidence.

Roles listed here are proposed responsibilities. They do not assign a named
individual or constitute a signature. A merged PR approves a repository change;
product release and regulatory approvals require their own controlled records.

## Repository evidence starting points

The following links locate implementation or draft documentation. Reviewers must
establish adequacy, traceability and actual test results before citing them as
evidence of conformity.

| Area | Starting points | Evidence still needed for a release assessment |
|---|---|---|
| Product and design | [PRD](../PRD.md), [SRS](../SRS.md), [SDS](../SDS.md), [architecture](../ARCHITECTURE.md) | Approved intended use, reconciled architecture and complete traceability |
| Build and dependencies | [CMake](../../CMakeLists.txt), [setup](../../setup.sh), [Windows setup](../../setup.ps1), [vcpkg manifest](../../vcpkg.json), [registry configuration](../../vcpkg-configuration.json), [CI](../../.github/workflows/ci.yml) | Frozen dependency versions, build provenance, SBOM, supplier assessments and release records |
| DICOM and geometry | [DICOM core](../../src/core/dicom), [coordinates](../../src/services/coordinate), [enhanced DICOM](../../src/services/enhanced_dicom) | Representative datasets, ground truth and numerical acceptance criteria |
| Rendering and browser | [render services](../../src/services/render), [client](../../client/src), [server API](../../server/src/api) | Complete clinical workflows, transport correctness, display and usability validation |
| Quantitative functions | [measurement](../../src/services/measurement), [flow](../../src/services/flow), [cardiac](../../src/services/cardiac), [export](../../src/services/export) | Function-specific analytical and clinical validation for each included claim |
| PACS | [PACS services](../../src/services/pacs), [integration reference](../reference/05-pacs-integration.md) | Product DICOM conformance statement and interoperability results for supported peers |
| Security and operations | [authentication](../../src/services/auth), [audit](../../src/services/pacs/audit_service.cpp), [stores](../../src/services/store), [deployment configuration](../../config), [server](../../server/src) | Threat model, security verification, deployment acceptance and incident exercises |
| Verification | [unit tests](../../tests/unit), [integration tests](../../tests/integration), [test CMake](../../tests/CMakeLists.txt) | Executed reports for the release candidate, coverage rationale and anomaly disposition |

## Baseline gaps to resolve

These observations refer to source commit
`172b4ec901a4df7dfbff82045832d249ab6ddd08`. They are review inputs, not a completed
conformity assessment. Recheck them against the actual release candidate.

| ID | Observation | Required follow-up |
|---|---|---|
| DV-GAP-01 | [README](../../README.md) records the September 2026 archive decision and unverified feature and performance claims. | Establish accountable maintenance and release scope before clinical distribution. |
| DV-GAP-02 | [Architecture](../ARCHITECTURE.md) and parts of [SRS](../SRS.md) describe Qt structures, while [server CMake](../../server/CMakeLists.txt) and [client](../../client/package.json) implement a web product. | Reconcile requirements, design, interfaces, deployment and validation scope. |
| DV-GAP-03 | CI pins only the network repository; setup scripts fetch moving revisions. CMake manually discovers libraries and CI caches PACS without dependency hashes. | Freeze all dependencies, use verified package contracts and test a clean build of the complete product. |
| DV-GAP-04 | [Export routes](../../server/src/api/export_routes.cpp) return queued jobs without a completed export pipeline; polling returns `not_ready`. | Implement and validate the included workflow, or remove its release claim and user access. |
| DV-GAP-05 | [Flow](../../server/src/api/flow_routes.cpp) and [cardiac](../../server/src/api/cardiac_routes.cpp) routes include placeholder results. | Trace every claimed measurement from browser input to calculation and displayed/exported output. |
| DV-GAP-06 | This repository does not establish the manufacturer's QMS operation, approved risk file, clinical evidence or signed release decision. | Obtain controlled records; distinguish records not supplied from confirmed missing records. |
| DV-GAP-07 | Version labels differ among [README](../../README.md), [CMake](../../CMakeLists.txt) and [client](../../client/package.json). | Define product and component identifiers and demonstrate their mapping to the assessed binaries. |

## Release readiness gates

These are proposed project gates, subject to adoption by the release authority.
Each gate needs a recorded disposition for the selected market and release.

- [ ] **DV-GATE-01** Approve all applicable product, classification and standard decisions.
- [ ] **DV-GATE-02** Reconcile the baseline gaps and freeze the complete release configuration.
- [ ] **DV-GATE-03** Complete traceability, verification, product and clinical validation for every included claim.
- [ ] **DV-GATE-04** Review residual safety, usability and security risks and all unresolved anomalies.
- [ ] **DV-GATE-05** Complete applicable QMS, GMP, submission, labeling and authorization activities.
- [ ] **DV-GATE-06** Approve distribution, installation, support, vulnerability response, field action and retirement arrangements.

Perform these reviews in order: scope and applicability; QMS and planning;
software and risk; usability and security; product validation; submission and
release. A dependency update reopens every affected review, including clinical
or usability validation when its impact analysis calls for it.
