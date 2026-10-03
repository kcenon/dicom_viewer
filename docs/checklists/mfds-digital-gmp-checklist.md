---
doc_id: DV-CHK-MFDS-GMP
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# MFDS digital medical device GMP checklist

Candidate basis: MFDS Notice No. 2025-28 on digital medical device manufacturing
and quality management, particularly Annex 2 (QMS), Annex 3 (software) and
conditional Annex 4 (AI controls). Confirm applicable annexes, audit type,
product group, exemptions and current rules before assessment. Guidance helps
interpret the notice; it is not interchangeable with the binding text.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.
Shared evidence may support multiple reviews, but an ISO checklist does not
automatically establish an MFDS GMP finding.

## Applicability and organizational controls

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MG-01 | Notice scope and audit route | Determine legal manufacturer/importer roles, product group, software form, assessment route and applicable annexes. | Approved GMP applicability and audit-scope decisions. |
| DV-MG-02 | Annex 4 applicability | Assess AI/ML actually supplied or connected. Do not infer AI from segmentation or infer absence from an unused optional library. | Function/model inventory, learning behavior and approved AI applicability decision. |
| DV-MG-03 | Annex 2, Clause 4 | Establish the QMS, outsourced-process controls, device file, document control, record retention and QMS software validation. | Controlled procedures and operational records, cross-referenced to the ISO 13485 review. |
| DV-MG-04 | Annex 2, Clauses 5 and 6 | Assign management responsibility, review performance and provide qualified staff and controlled infrastructure. | Policy, objectives, reviews, competence, resources and environment records. |
| DV-MG-05 | Annex 2, Clauses 7.1 through 7.4 | Control product planning, customer requirements, design activities and suppliers. | Design history, requirements reviews, supplier agreements and acceptance records. |
| DV-MG-06 | Annex 2, Clauses 7.5 and 7.6 | Control build/release, installation, servicing, identification, traceability, customer data and verification tools. | Deployment traceability, process/tool validation, data protection and justified exclusions for physical-device provisions. |
| DV-MG-07 | Annex 2, Clause 8 | Operate feedback, complaint/reporting, audit, nonconformity, data analysis and corrective/preventive action processes. | Actual QMS records and effectiveness evidence. |

## Software design and development

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MG-08 | Annex 3, 1.1 | Plan development, verification, validation and maintenance with responsibilities and acceptance criteria. | Approved plans covering browser, C++ server and PACS dependencies. |
| DV-MG-09 | Annex 3, 1.2 | Define, review and maintain system requirements, customer expectations and operating environment. | Approved requirements, review history and resolution of obsolete architecture descriptions. |
| DV-MG-10 | Annex 3, 1.3 | Control design inputs/outputs, architecture, SOUP, interfaces, segregation and detailed unit design. | Reviewed design and allocation of safety/security controls. |
| DV-MG-11 | Annex 3, 1.3 | Verify units and control design transfer and changes. | Unit verification, transfer review and approved change records. |
| DV-MG-12 | Annex 3, 1.4 | Execute integration and system tests on the full release configuration, preserving repeatable records and anomalies. | Tests covering PACS, client/server state, geometry, measurements and enabled exports. |
| DV-MG-13 | Annex 3, 1.4 | Validate the intended product, including user interaction and quantitative clinical claims. | Approved validation results with representative data, users and environments. |
| DV-MG-14 | Annex 3, 1.5 | Authorize deployment and preserve release artifacts and the method used to build them. | Release decision, source/dependency hashes, SBOM, artifact checks and retained records. |
| DV-MG-15 | Annex 3, 1.6 | Document feedback assessment, maintenance and problem resolution throughout support. | Maintenance records, impact analysis and verified corrections. |

## Software support processes

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MG-16 | Annex 3, 2.1 | Plan and perform risk analysis, evaluation, controls, verification and change assessment. | Risk file and bidirectional traceability for software contributions to harm. |
| DV-MG-17 | Annex 3, 2.1 | Justify any legacy-software route and close its documented gaps. | Eligibility, post-production information, gap-remediation and continued-use records. |
| DV-MG-18 | Annex 3, 2.2 | Control outsourced software/services and acceptance of supplied components, including external PACS and cloud/identity dependencies. | Outsourcing responsibilities, acceptance criteria and verification results. |
| DV-MG-19 | Annex 3, 2.3 | Identify and control configurations, changes and record retention across all supported releases. | Common dependency baseline for CI/setup, reproducible builds and change history. |
| DV-MG-20 | Annex 3, 2.4 | Record, investigate, communicate, trend and verify resolution of problems. | Defect/CAPA records, risk decisions and complete regression evidence. |

## Conditional AI controls

Assess each row independently if AI is included. Otherwise retain an approved
non-applicability rationale. Future AI integration reopens these decisions.

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MG-21 | Annex 4, 1 through 3 | Establish AI policy, responsibilities, resources, risk management and governance for the supplied model/service. | AI management scope, competence, risk and accountability records. |
| DV-MG-22 | Annex 4, 4 | Plan reliability, intended performance, limitations and evaluation across relevant populations and acquisition settings. | Reliability plan and predefined acceptance criteria. |
| DV-MG-23 | Annex 4, 5 | Control training/evaluation data provenance, rights, quality, labeling, representativeness and dataset separation. | Data governance records, dataset versions and leakage/bias assessments. |
| DV-MG-24 | Annex 4, 6 and 7 | Control model development and system integration, preserving model versions and validation of the deployed behavior. | Model/software traceability, performance evidence and integration verification. |
| DV-MG-25 | Annex 4, 8 | Monitor operational performance, drift, safety and security; control retraining, updates, rollback and retirement. | Monitoring plan/results, change approvals and revalidation records. |

## Evidence starting points

- [ISO 13485](iso-13485-checklist.md), [IEC 62304](iec-62304-checklist.md), [risk](iso-14971-checklist.md) and [product validation](iec-82304-1-checklist.md).
- [Build configuration](../../CMakeLists.txt), [CI](../../.github/workflows/ci.yml), [deployment](../../config), [tests](../../tests).
- Organization-level records and GMP certificates must be supplied by their controlled owners.
