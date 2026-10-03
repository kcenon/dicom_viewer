---
doc_id: DV-CHK-MFDS-APPROVAL
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# MFDS digital medical device approval checklist

Use this checklist if South Korea is a target market and the product is
determined to fall within the applicable digital medical device framework.
Candidate references are the classification notice amended by No. 2026-4 and
the approval notice amended by No. 2026-54. Confirm the current consolidated
texts, effective dates, transition rules and application forms in the
[reference register](regulatory-references.md) before an assessment.

This package selects no product code, device class, authorization route or
clinical-evidence exemption. A regulatory device class and an IEC 62304 software
safety class are different decisions. Use the
[assessment rules](README.md#how-to-record-a-review) and
[review record](review-record-template.md). Every row is initially unassessed.

## Qualification and classification

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MA-01 | Market and qualification | Confirm the legal manufacturer, target market and medical purpose of the supplied product. | Approved qualification and market decision, including research-only exclusions where relevant. |
| DV-MA-02 | Classification notice, Articles 2 through 8 and annexes | Define the standalone/embedded software boundary and each supplied function, accessory, interface and required infrastructure. | Product composition and function inventory, with software and hardware responsibilities. |
| DV-MA-03 | Classification notice, intended purpose | Describe indications, population, clinical setting, users and how each displayed or calculated output affects clinical decisions. | Approved intended use aligned with PRD, UI, instructions and promotional claims. |
| DV-MA-04 | Classification notice, product code and class | Justify product codes and classes from the adopted classification rules, including multiple functions and any unclassified case. | Classification rationale, current rule references and authority consultation when needed. |
| DV-MA-05 | Approval notice, application route | Determine approval, certification or notification route, responsible body and manufacturer/importer obligations. | Regulatory strategy, required establishment records and submission responsibilities. |
| DV-MA-06 | Approval notice, review scope | Identify any function or infrastructure excluded from technical review without dropping its safety or interoperability impact. | Function-level scope and exclusion rationale tied to the product architecture. |

## Product description and application content

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MA-07 | Articles 9 through 12 | Identify product/model names, representative model, component software names, versions and operating environments. | Controlled product/component identifiers and representative-configuration rationale. |
| DV-MA-08 | Article 11 | Describe operating principles, screens, functions, communication architecture, interoperable devices and applied security measures. | Architecture diagrams, annotated final UI screens, PACS interfaces and protection descriptions. |
| DV-MA-09 | Articles 14, 15 | State clinically relevant performance and intended purpose for each included measurement, segmentation, flow or cardiac function. | Claims-to-performance mapping and algorithm/scientific rationale. |
| DV-MA-10 | Articles 16, 17 | Prepare operating instructions and precautions that match the final UI, environment limits, interactions and cybersecurity responsibilities. | Reviewed instructions for use with screenshots, warnings and limitations. |
| DV-MA-11 | Article 18 | Define safety and performance test methods, acceptance criteria and standards for each function and configuration. | Approved test specifications linked to classification and risk decisions. |

## Submission evidence

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MA-12 | Articles 24 through 26 | Provide intended-purpose, operating-principle, development-history and comparable-product information as applicable. | Dossier sections with controlled sources and applicability rationale. |
| DV-MA-13 | Software verification and validation | Assemble requirements, architecture, implementation, verification and validation evidence for the actual release candidate. | Traceability, executed reports, residual anomalies and configuration identifiers. |
| DV-MA-14 | Clinical evaluation materials | Determine the clinical evidence needed for each claim and acceptable study/source types; justify datasets, comparators and acceptance criteria. | Approved clinical evaluation plan and evidence, including sample rationale and limitations. |
| DV-MA-15 | Articles 25, 27, 28 | Assess any exemption, substitution or reliance pathway against its conditions and retain the decision. | Regulatory rationale and supporting certificates/records; no inferred exemption from unit tests or similar-product claims. |
| DV-MA-16 | Cybersecurity materials | Assemble protection measures and evaluation results for actual communication interfaces and use environments. | Completed [MFDS cybersecurity review](mfds-digital-cybersecurity-checklist.md) and product-specific evidence. |
| DV-MA-17 | Usability materials | Provide evaluation of the final use method, UI and instructions for intended users. | Completed [usability review](iec-62366-1-checklist.md), protocols, results and risk linkage. |
| DV-MA-18 | Professional-use and AI conditions | Determine professional-use labeling and any conditional AI change-management submission. | Decision records, qualified-user instructions and AI plan/evidence if applicable. |
| DV-MA-19 | GMP and evidence acceptance | Verify applicable GMP status and that all submitted reports identify the correct product, model, version and representative configuration. | GMP records, report provenance/validity and dossier consistency review. |

## Software conformity report, changes and labeling

| ID | Reference area | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-MA-20 | Approval notice, Annex 1 | Complete product identity, function types, operating environment, software safety class and supporting document identifiers. | Controlled software conformity report consistent with IEC 62304; resolve any conflicting class definitions before sign-off. |
| DV-MA-21 | Annex 1, lifecycle summaries | Summarize development, maintenance, risk and configuration activities using approved records, not prospective plans alone. | Report cross-reference index to completed technical and QMS records. |
| DV-MA-22 | Changes after authorization | Assess whether changes to pacs_system, algorithms, platform, deployment or intended use require a regulatory action before distribution. | Change assessment, required filing/approval records and verified updated release. |
| DV-MA-23 | Digital Medical Products Act, Article 22; implementing rule, Article 33 | Determine applicable manufacturer/importer, product/model, authorization, version, software/professional-use and identification labeling. | Labeling matrix and verified package/UI/documentation presentation; AI disclosures if applicable. |
| DV-MA-24 | Post-market obligations | Assign complaint, incident, vigilance, renewal and field-action responsibilities with applicable triggers and timelines. | Approved procedures, authority/provider contacts and retained decision records. |

## Evidence starting points

- [PRD](../PRD.md), [SRS](../SRS.md), [SDS](../SDS.md), [API implementation](../../server/src/api), [client](../../client/src).
- [Product validation](iec-82304-1-checklist.md), [MFDS GMP](mfds-digital-gmp-checklist.md), [risk](iso-14971-checklist.md).
- Obtain legal manufacturer, QMS, regulatory consultation and authorization records from their controlled owners.
