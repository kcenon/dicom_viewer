---
doc_id: DV-CHK-62366
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# IEC 62366-1 usability engineering checklist

Candidate basis: IEC 62366-1:2015 with Amendment 1:2020, Edition 1.1, Clauses 4
and 5 and conditional Annex C. Review safety-related use of the actual browser
product with its intended users and environments. Developer demonstrations and
automated UI tests are inputs; they do not replace evaluation with representative
users when that evaluation is required.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Process and use specification

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62366-01 | 4.1, 4.2 | Establish the usability process and file, linking UI risk controls to the device risk file. | Approved plan, competence, responsibilities and usability-file index. |
| DV-62366-02 | 4.1 | Prefer safer interface design over warnings alone; evaluate whether users perceive and understand safety information. | Design rationale, warning requirements and evaluation results. |
| DV-62366-03 | 4.3 | Tailor effort to the significance of the UI and its risks without omitting necessary safety evaluation. | Documented scope and resource rationale. |
| DV-62366-04 | 5.1 | Define users, clinical tasks, patient population, environments, language, training and accessibility needs. | Approved use specification for clinical and any separately controlled research use. |
| DV-62366-05 | 5.2, 5.3 | Identify safety-related UI characteristics and known use errors, including similar-product experience. | Task analysis, complaint/literature review and linked hazards. |

## Hazard-related scenarios and interface design

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62366-06 | 5.4, 5.5 | Define and select hazard-related use scenarios for summative evaluation using a documented rationale. | Scenario inventory, risk linkage and selection decision. |
| DV-62366-07 | 5.4, 5.6 | Make patient, study, series, acquisition time, orientation and active viewport unambiguous during viewing and PACS retrieval. | UI requirements and scenarios for wrong-patient/series and laterality errors. |
| DV-62366-08 | 5.4, 5.6 | Distinguish live, loading, stale, disconnected and failed render states; prevent old frames being mistaken for current results. | Interaction/state requirements and slow-network, reconnection and session-expiry scenarios. |
| DV-62366-09 | 5.4, 5.6 | Make measurement units, calibration, edit state, segmentation ownership and undo/delete consequences clear. | Measurement and segmentation task scenarios with acceptance criteria. |
| DV-62366-10 | 5.4, 5.6 | Present phase, VENC, acquisition limitations, algorithm assumptions and invalid analysis results intelligibly. | Flow/cardiac scenarios and verified labels, warnings and result provenance. |
| DV-62366-11 | 5.4, 5.6 | Differentiate completed exports and committed saves from queued, partial or failed operations. | End-to-end export/save/retrieve scenarios and feedback requirements. |
| DV-62366-12 | 5.6 | Specify input mappings, focus, keyboard shortcuts, browser zoom, display scaling and multi-viewport behavior. | UI specification tied to supported browser, display and input configurations. |

## Evaluation and existing interfaces

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62366-13 | 5.7 | Plan formative and summative methods, users, environments, training, scenarios, data collection and success criteria before execution. | Approved evaluation plans with recruitment and sampling rationale. |
| DV-62366-14 | 5.8 | Conduct formative evaluations during design and track the effect of resulting changes. | Observation records, design changes and follow-up evaluation. |
| DV-62366-15 | 5.9 | Conduct summative evaluation on the representative final UI and included clinical workflows. | Executed protocol, participant characteristics, results and deviations. |
| DV-62366-16 | 5.9 | Analyze observed use errors, close calls and difficulties; assess root causes and remaining risks. | Usability conclusions, corrective actions, risk updates and justified retesting. |
| DV-62366-17 | 5.10, Annex C | Decide whether any UI qualifies for the interface-of-unknown-provenance route; do not infer eligibility from archive status. | Applicability rationale and identification of affected UI versions. |
| DV-62366-18 | Annex C | If applicable, evaluate use specification, post-production information, hazards, controls and residual risks for the existing UI. | Annex C assessment and any additional evaluation needed to close gaps. |

## Evidence starting points

- [Browser client](../../client/src), [render services](../../src/services/render), [server API](../../server/src/api).
- [PRD](../PRD.md) personas are candidate inputs to the approved use specification.
- Reconcile findings with [risk management](iso-14971-checklist.md) and [product validation](iec-82304-1-checklist.md).
