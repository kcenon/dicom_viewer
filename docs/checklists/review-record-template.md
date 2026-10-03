---
doc_id: DV-CHK-RECORD
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Template
product: dicom_viewer
---

# SaMD checklist review record template

Copy this template for a specific assessment. Fill every field or state why it
is undecided. Retain the approved assessment in the designated QMS location;
this blank template contains no assessment or approval.

## Assessment identity

| Field | Value to enter |
|---|---|
| Record identifier and revision | TBD |
| Review purpose and target market | TBD |
| Product name, model, release and artifact hashes | TBD |
| Repository and full source SHA | TBD |
| Dependency manifest, SBOM revision and all ecosystem SHAs | TBD |
| Browser, OS, GPU, server, storage and network configuration | TBD |
| Included and excluded functions | TBD |
| Checklist document IDs, versions and full checklist source SHA | TBD |
| Standards and regulations, adopted editions and controlled source identifiers | TBD |
| Applicability and safety classification decision records | TBD |
| Review date, reviewer name, role and competence record | TBD |
| Selected checklist IDs and omitted IDs | TBD |
| External records requested and received, including revisions | TBD |
| Evidence retention location and access controls | TBD |

## Assessment vocabulary

| Field | Allowed values and meaning |
|---|---|
| Applicability | `Undecided`, `Applicable`, `Not applicable` |
| Finding | `Not reviewed`, `Pending evidence`, `Gap`, `Satisfied`, `Not applicable` |
| Evidence availability | `Not assessed`, `Available`, `Partial`, `External requested`, `Confirmed missing` |

Use `Not applicable` only with an approved applicability rationale. Do not use
`Satisfied` while applicability is undecided, evidence is missing, or an
applicable part of a grouped prompt remains open. `External requested` means
the evidence has not been supplied; it does not establish that it does not
exist. Code, a plan, a blank report and an executed test report are different
types of evidence.

## Item assessments

Repeat the row for every selected checklist ID. Preserve IDs when splitting a
grouped prompt, for example by appending `.1` and `.2`, and retain a parent
assessment. Link each item to the action register when follow-up is needed.

| Checklist ID | Applicability | Finding | Evidence availability | Evidence identifier and revision | Rationale and action ID |
|---|---|---|---|---|---|
| Enter selected ID | Undecided | Not reviewed | Not assessed | TBD | TBD |

## Actions and traceability

| Action ID | Checklist IDs | Requirement and risk IDs | Action and acceptance criteria | Owner | Due date | Closure evidence and reviewer |
|---|---|---|---|---|---|---|
| TBD | TBD | TBD | TBD | TBD | TBD | TBD |

Retain links from intended use and user needs to requirements, hazards, risk
controls, design items, source revisions, test procedures, results and residual
risk decisions. Record any affected submission or field action decision.

## Review outcome

| Field | Value to enter |
|---|---|
| Applicable items satisfied, open and not reviewed | TBD |
| Non-applicable items and approval references | TBD |
| Unresolved anomalies and residual risk decision references | TBD |
| Scope and limits of the conclusion | TBD |
| Release or submission decision and authority | TBD |
| Actual reviewer and approver signatures, dates or controlled record references | TBD |
| Conditions that require reassessment | TBD |

A checklist completion date is not a market authorization date. Product release
approval and any authority decision must be identified separately.
