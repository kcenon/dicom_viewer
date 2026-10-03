---
doc_id: DV-CHK-14971
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# ISO 14971 risk management checklist

Candidate basis: ISO 14971:2019, Clauses 4 through 10. Assess the device across
its lifecycle and intended environment. ISO/TR 24971:2020 is supporting guidance,
not an additional set of normative requirements. The examples below are prompts
for hazard analysis; no severity, probability, acceptability or safety class has
been assigned.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Risk process and analysis

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-14971-01 | 4.1 through 4.3 | Establish the risk process, management policy, resources and competent reviewers for imaging and clinical use. | Approved policy, responsibilities, competence and process review records. |
| DV-14971-02 | 4.4 | Plan scope, responsibilities, review points, verification, acceptability criteria and overall residual-risk evaluation before judging results. | Approved risk management plan, including cases where probability cannot be estimated. |
| DV-14971-03 | 4.5 | Maintain a risk file that connects each hazard and hazardous situation to evaluation, controls, verification and residual risk. | Risk-file index and complete traceability. |
| DV-14971-04 | 5.1, 5.2 | Define intended medical use, patient population, user qualifications and foreseeable misuse, including use of research or incomplete functions in diagnosis. | Approved intended use, exclusions, misuse analysis and clinical review. |
| DV-14971-05 | 5.3, 5.4 | Analyze wrong patient, study, series, frame or cardiac phase selection and identifier mismatch during PACS retrieval, caching and export. | Hazard sequences and controls for identity and result provenance. |
| DV-14971-06 | 5.3, 5.4 | Analyze left/right reversal, LPS/RAS mismatch, incorrect orientation, spacing, resampling and oblique/MPR transforms. | Geometry hazards and control specifications with reference datasets. |
| DV-14971-07 | 5.3, 5.4 | Analyze pixel signedness, rescale/HU conversion, photometric interpretation, lossy encoding and incorrect window/level. | Image-fidelity analysis and input/processing limitations. |
| DV-14971-08 | 5.3, 5.4 | Analyze plausible but wrong segmentations, distances, areas, volumes and ROI statistics, including manual editing and unit conversion. | Measurement hazards, user review controls and accuracy requirements. |
| DV-14971-09 | 5.3, 5.4 | Analyze vendor VENC interpretation, aliasing correction, temporal alignment and unsupported acquisition protocols in 4D Flow. | Flow analysis hazards, validity checks and claim-specific limitations. |
| DV-14971-10 | 5.3, 5.4 | Analyze phase detection, calcium thresholds, reconstruction settings, centerlines and derived cardiac measurements. | Cardiac analysis hazards and constraints on validated acquisitions. |
| DV-14971-11 | 5.3, 5.4 | Analyze stale or misrouted streamed frames, dropped interactions, overload, GPU failure and interrupted retrieval. | Timing/session hazards, resource limits, detection and recovery controls. |
| DV-14971-12 | 5.3, 5.4 | Analyze unauthorized access, image/result modification, audit loss and availability attacks for their possible clinical consequences. | Linked threat and safety analyses; failure sequences across trust boundaries. |
| DV-14971-13 | 5.3, 5.4 | Analyze truncated exports, wrong metadata/units, incomplete project saves and failed restores that appear successful. | Persistence and export hazards, integrity checks and user feedback requirements. |
| DV-14971-14 | 5.5, 6 | Estimate and evaluate risks using the approved method, addressing uncertainty and systematic software faults. | Documented estimates, assumptions and decisions; rationale beyond a CVSS score or passing test count. |

## Controls and residual risk

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-14971-15 | 7.1 | Consider safer design and protective measures before relying on warnings or user training. | Risk-control option analysis with selection rationale. |
| DV-14971-16 | 7.2 | Verify both implementation and effectiveness of each control, including identity, geometry, stale-frame handling and limits on unsupported inputs. | Control-specific verification with predefined acceptance criteria. |
| DV-14971-17 | 7.3 | Evaluate individual residual risks and determine what must be disclosed to users. | Residual-risk decisions and traceable safety information. |
| DV-14971-18 | 7.4 | Where further reduction is impracticable and criteria are not met, assess benefit against residual risk using relevant clinical evidence. | Documented benefit-risk analysis and authorized conclusion. |
| DV-14971-19 | 7.5, 7.6 | Check for new risks from controls and confirm all hazardous situations have been addressed. | Completeness review and analysis of control interactions. |
| DV-14971-20 | 8 | Evaluate overall residual risk for the complete product, including interactions among measurement, display and network limitations. | Overall evaluation against the plan, clinical input and required disclosures. |
| DV-14971-21 | 9 | Review execution of the plan, acceptability and readiness to collect production/post-production information before release. | Approved risk management report and release linkage. |

## Production and post-production

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-14971-22 | 10.1, 10.2 | Collect complaints, clinical incidents, near misses, supplier defects, interoperability failures and new vulnerability information. | Information sources, owners, review frequency and field-data records. |
| DV-14971-23 | 10.3 | Evaluate new information for unidentified hazards, changed acceptability or changes to the benefit-risk conclusion. | Periodic and event-triggered risk review records. |
| DV-14971-24 | 10.4 | Take and verify actions affecting installed systems, technical documentation, users and future releases. | Field action/change records, notification decisions, effectiveness evidence and updated risk file. |

## Evidence starting points

- [DICOM core](../../src/core/dicom), [coordinates](../../src/services/coordinate), [rendering](../../src/services/render).
- [Measurements](../../src/services/measurement), [flow](../../src/services/flow), [cardiac](../../src/services/cardiac), [exports](../../src/services/export).
- [Usability](iec-62366-1-checklist.md) and [security](iec-81001-5-1-checklist.md) findings must feed the same safety risk decisions.
