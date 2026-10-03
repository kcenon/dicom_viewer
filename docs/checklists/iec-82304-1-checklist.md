---
doc_id: DV-CHK-82304
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# IEC 82304-1 health software product checklist

Candidate basis: IEC 82304-1:2016, Clauses 4 through 8. Review the complete
health software product and its intended computing environment. A C++ library
test, successful CI run or browser demonstration does not by itself establish
validation of the product for its intended use.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Product requirements and lifecycle

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-82304-01 | 4 | Approve intended purpose, intended users, clinical context and product boundaries, including excluded research functions. | Product requirements and claims matrix with clinical/regulatory review. |
| DV-82304-02 | 4 | Define functional, performance, safety, security, usability and interoperability requirements at product level. | Reviewed requirements with measurable acceptance criteria. |
| DV-82304-03 | 4 | Define supported browser, server OS, CPU/GPU, display, network, PACS, identity and storage environments. | Compatibility matrix and installation/environment requirements. |
| DV-82304-04 | 4 | Define dependencies on external systems and behavior when a prerequisite becomes unavailable or incompatible. | External interface contracts, shared responsibilities and safe failure requirements. |
| DV-82304-05 | 4 | Evaluate product risk, requirement completeness and consistency; maintain traceability through changes. | Requirements reviews and linked risk/verification records. |
| DV-82304-06 | 5 | Apply the selected software lifecycle activities to every supplied component and support the product requirements. | Completed IEC 62304 assessment and product-to-software traceability. |

## Product validation

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-82304-07 | 6.1 | Plan validation before execution, defining methods, intended users, environments, datasets, acceptance criteria and responsibilities. | Approved product validation plan and independence/competence rationale. |
| DV-82304-08 | 6.2 | Validate loading, viewing, MPR and rendering with representative modalities, orientations, enhanced multiframe data and invalid inputs. | Reference images, expected geometry/pixels, comparison methods and results. |
| DV-82304-09 | 6.2 | Validate segmentation and quantitative measurements against justified reference methods and clinically relevant tolerances. | Ground truth, sample rationale, accuracy/repeatability and boundary-case results. |
| DV-82304-10 | 6.2 | Validate each included flow/cardiac claim across supported acquisition protocols and vendors. Prevent unsupported or placeholder outputs from appearing clinically valid. | Analytical and clinical evidence for the actual claims, failure detection and limitations. |
| DV-82304-11 | 6.2 | Validate full PACS, DICOMweb, project persistence and export workflows, preserving patient identity, units and provenance. | End-to-end results with supported peers and exported artifacts. |
| DV-82304-12 | 6.2 | Validate concurrent use, poor networks, reconnection, GPU/resource exhaustion and recovery without misleading display state. | Capacity and interruption results on the claimed deployment configurations. |
| DV-82304-13 | 6.2 | Evaluate safety-related use with representative users and final user instructions. | Usability evaluation and product validation linkage. |
| DV-82304-14 | 6.3 | Report the tested product and environment, methods, results, deviations, anomalies, conclusions and responsible personnel. | Approved validation report with traceability to requirements and retained source evidence. |

## Identification and accompanying documents

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-82304-15 | 7.1 | Identify the manufacturer, product, model and release consistently in the UI, package, documentation and support records. | Product/component version mapping and label/about-screen verification. |
| DV-82304-16 | 7.2.1, 7.2.2 | Provide intended purpose, users, limitations, warnings, operating instructions, interpretation of outputs and support contacts. | Reviewed instructions for use and language/readability/usability evidence. |
| DV-82304-17 | 7.2.3 | Provide technical requirements, installation, network ports, identity/storage setup, hardening, backup, updates and decommissioning instructions. | Technical description and tested deployment/service procedures. |
| DV-82304-18 | 7.2 | Reconcile all required documentation items with the adopted standard and applicable labeling rules, including any omitted conditional items. | Documentation coverage matrix, approved exclusions and release-document index. |

## Post-release activities

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-82304-19 | 8 | Maintain complaint, defect, security and performance monitoring with defined support and customer communications. | Maintenance plan, field-feedback reviews and escalation records. |
| DV-82304-20 | 8 | Reassess and revalidate affected product claims after changes; plan migration and safe retirement. | Change impact, verification/validation results, update instructions and retirement/data-transfer records. |

## Evidence starting points

- [PRD](../PRD.md), [SRS](../SRS.md), [deployment configurations](../../config), [server](../../server), [client](../../client).
- [Unit](../../tests/unit) and [integration](../../tests/integration) tests provide inputs to the validation plan.
- Review [baseline gaps](README.md#baseline-gaps-to-resolve) before selecting a release candidate.
