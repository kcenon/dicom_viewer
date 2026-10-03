---
doc_id: DV-CHK-13485
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# ISO 13485 quality management checklist

Candidate basis: ISO 13485:2016, Clauses 4 through 8. This review concerns the
manufacturer's quality management system (QMS), including outsourced activities.
Repository files alone cannot demonstrate that the organization operates these
processes. Request controlled external records without treating unavailable
records as confirmed absence.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially
unassessed. Evaluate exclusions against the actual supplied product; a software
repository does not automatically exclude all hardware or service obligations.

## QMS, management and resources

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-13485-01 | 4.1 | Define QMS scope, processes, interactions, regulatory roles, outsourced work and controls over process changes. | Quality manual, process map and approved applicability/exclusion rationale. |
| DV-13485-02 | 4.1.6 | Validate software used by the QMS according to its intended use and risk, including electronic approvals and test/report tooling where applicable. | Tool inventory, validation rationale, execution records and change reassessment. |
| DV-13485-03 | 4.2 | Maintain the medical device file and control creation, review, approval, revision, access, retention and retrieval of records. | Document-control procedure and actual device-file and record indexes. |
| DV-13485-04 | 5.1 through 5.5 | Establish management commitment, quality policy, measurable objectives, customer/regulatory focus and authority for release and escalation. | Approved policy, objectives, role assignments and communication records. |
| DV-13485-05 | 5.6 | Conduct management reviews covering audits, complaints, field performance, suppliers, changes, risks and resource needs. | Review inputs, decisions, assigned actions and follow-up records. |
| DV-13485-06 | 6.1, 6.2 | Provide competent personnel for software, clinical evaluation, usability, security, quality and regulatory activities. | Competence requirements, training/effectiveness records and resource plans. |
| DV-13485-07 | 6.3, 6.4 | Control infrastructure and work environments used for builds, validation, hosting, support and access to patient datasets. | Environment controls, maintenance records and any applicable contamination-control decision. |

## Product realization and design

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-13485-08 | 7.1, 7.2 | Review user, clinical, regulatory, delivery and support requirements before commitments to hospitals or distributors. | Product realization plan, contract/requirements reviews and customer communications. |
| DV-13485-09 | 7.3.1 through 7.3.4 | Plan design activities and define approved inputs and outputs for each release configuration and claimed function. | Development plan, design inputs, outputs and acceptance criteria. |
| DV-13485-10 | 7.3.5 | Hold documented design reviews with the required functions and address resulting actions. | Review attendance, decisions, issues and closure evidence. |
| DV-13485-11 | 7.3.6 | Verify design outputs against inputs, including interface compatibility and quantitative imaging results. | Verification procedures, traceability and executed results. |
| DV-13485-12 | 7.3.7 | Validate the product for its intended use with representative users, data and environments; justify clinical evidence and sample selection. | Approved validation/clinical plans and reports, rationale and unresolved limitations. |
| DV-13485-13 | 7.3.8 | Transfer approved design into a repeatable build, distribution, installation and support process. | Transfer review, reproducible artifacts, installation acceptance and service readiness records. |
| DV-13485-14 | 7.3.9, 7.3.10 | Control design changes and preserve the design history, including PACS/SOUP updates and changes to clinical claims. | Design file, change impact, verification/validation and approval records. |

## Suppliers, delivery and traceability

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-13485-15 | 7.4 | Evaluate suppliers and purchased/outsourced services according to risk. Define acceptance and change-notification controls for libraries, cloud services and contractors. | Supplier evaluations, agreements, dependency acceptance and monitoring records. |
| DV-13485-16 | 7.5.1 through 7.5.4 | Control release production, installation and servicing; assess cleanliness requirements for anything supplied with the software. | Release/installation procedures, site acceptance and service records; applicability rationale. |
| DV-13485-17 | 7.5.5 through 7.5.7 | Assess sterilization provisions and validate production/service processes whose outputs cannot be fully verified afterward. | Documented applicability, process validation and revalidation decisions; no blanket software exemption. |
| DV-13485-18 | 7.5.8, 7.5.9 | Identify release status and trace installed artifacts and configurations to sites, including any additional implantable-device obligations if applicable. | Release inventory, deployment history, traceability and justified exclusions. |
| DV-13485-19 | 7.5.10, 7.5.11 | Protect customer images, identifiers and project files during handling, support, backup, retention and disposal. | Customer-property controls, data-loss handling, integrity checks and recovery records. |
| DV-13485-20 | 7.6 | Control reference datasets, phantoms, displays, measuring equipment and software used to establish accuracy. | Calibration/qualification where applicable, tool validation and invalid-result impact assessments. |

## Measurement and improvement

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-13485-21 | 8.1, 8.2.1 through 8.2.3 | Collect feedback, investigate complaints and make documented regulatory reporting decisions. | Monitoring methods, complaint files, reportability reviews and notifications. |
| DV-13485-22 | 8.2.4 through 8.2.6 | Audit QMS processes and monitor product/process performance with authority for acceptance decisions. | Audit plans/results, metrics, acceptance records and follow-up. |
| DV-13485-23 | 8.3 | Control nonconforming builds before and after delivery, including quarantine, correction, rework and field actions. | Nonconformity records, affected-release assessment, re-verification and customer action records. |
| DV-13485-24 | 8.4, 8.5 | Analyze trends and perform corrective and preventive action, checking effectiveness and unintended safety effects. | Data analysis, CAPA records, effectiveness checks and linked risk updates. |

## Evidence starting points

- [Product requirements](../PRD.md), [software requirements](../SRS.md), [design](../SDS.md), [tests](../../tests).
- [Software lifecycle checklist](iec-62304-checklist.md), [MFDS GMP checklist](mfds-digital-gmp-checklist.md).
- Obtain company policies, audit, supplier, competence, complaint and approval records from the controlled QMS.
