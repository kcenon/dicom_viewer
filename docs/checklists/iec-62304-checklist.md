---
doc_id: DV-CHK-62304
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
product: dicom_viewer
---

# IEC 62304 software lifecycle checklist

Candidate basis: IEC 62304:2006 with Amendment 1:2015, Edition 1.1, Clauses 4
through 9. Determine the system and item safety classes before selecting
class-dependent activities. Product validation and final device release also
need the [product checklist](iec-82304-1-checklist.md).

Use the [assessment rules](README.md#how-to-record-a-review) and
[review record](review-record-template.md). Every row is initially unassessed.
Clause groups identify where to consult the adopted standard; grouped questions
need separate findings if their parts have different outcomes. See the
[reference register](regulatory-references.md) for edition control.

## Planning and general requirements

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62304-01 | 4.1, 4.2 | Place software development and maintenance under the manufacturer's QMS and risk process. | Approved process scope, responsibilities and risk management plan. |
| DV-62304-02 | 4.3 | Justify A, B or C for the complete system and any separately classified items. Consider incorrect images and measurements, delayed access and failure of external controls. | Safety classification decision, hazardous situations, severity rationale and segregation evidence. |
| DV-62304-03 | 4.4 | Determine whether the legacy software provisions actually apply. Repository age or archive status alone is not a justification. | Applicability decision; if applicable, feedback review, gap analysis, remediation and continued-use rationale. |
| DV-62304-04 | 5.1 | Define lifecycle activities, deliverables, review authorities and clinical release scope for the browser, server and PACS integration. | Approved development plan and maintained revisions. |
| DV-62304-05 | 5.1 | Plan integration order, verification methods, acceptance criteria and links to system validation. | Integration and verification plans, review schedule and traceability approach. |
| DV-62304-06 | 5.1 | Control development tools, coding rules, test tools, documentation, configuration, change requests and defect handling before verification. | Tool inventory, tool validation decisions, coding rules and configuration/problem-resolution procedures. |

## Requirements and design

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62304-07 | 5.2 | Reconcile PRD/SRS/SDS with the implemented web architecture and approved feature set. Define testable requirements with stable identifiers. | Reviewed requirements baseline and change history; resolution of outdated Qt and controller descriptions. |
| DV-62304-08 | 5.2 | Specify DICOM inputs, identity, geometry, pixel interpretation, supported transfer syntaxes and invalid-input behavior. | Interface and data requirements, supported-format matrix and acceptance criteria. |
| DV-62304-09 | 5.2 | Specify quantitative accuracy, units, coordinate transforms, phase selection and result provenance for every included analysis. | Numerical requirements for measurements, flow, cardiac and exported results. |
| DV-62304-10 | 5.2 | Include latency, capacity, interruption, security, installation, storage and hospital network requirements. Incorporate software risk controls. | Reviewed operational and security requirements linked to risks and tests. |
| DV-62304-11 | 5.3 | Describe client/server boundaries, session state, frame transport, authentication, storage and PACS interfaces. Verify the architecture against requirements. | Architecture and interface review, deployment diagrams and risk-control allocation. |
| DV-62304-12 | 5.3, 7.1 | Identify SOUP and other supplied components; define required behavior, platform assumptions and handling of known anomalies. | Component assessment for ITK, VTK, GDCM, Crow, Asio, OpenSSL, client packages and applicable kcenon libraries. |
| DV-62304-13 | 5.3 | Justify any separation used to reduce a software item's safety class, including shared memory, shared services and shared GPU resources. | Segregation design and verification; impact of common dependencies and resource exhaustion. |
| DV-62304-14 | 5.4 | Define units and detailed interfaces sufficiently to implement and review concurrent rendering, asynchronous requests and cancellation. | Detailed design, state/lifetime rules and design verification records. |

## Implementation and verification

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62304-15 | 5.5 | Verify units against predefined criteria, including bounds, numerical precision, initialization, memory ownership and error handling. | Unit verification results, code reviews and static/dynamic analysis appropriate to risk. |
| DV-62304-16 | 5.6 | Test the real browser-to-server-to-service path for each included workflow; distinguish a stub response from a completed operation. | Executed integration results for loading, rendering, segmentation, measurements, analysis and export. |
| DV-62304-17 | 5.6 | Test C-ECHO, C-FIND, C-MOVE, C-STORE, DICOMweb, SR and Storage Commitment where included, with supported peers. | Interoperability matrix, protocol traces, identity checks, timeouts and recovery results. |
| DV-62304-18 | 5.6 | Verify imported package targets, transitive libraries, feature definitions and ABI consistency after ecosystem updates. | Clean configure/build/link results using the frozen dependency set; runtime smoke results. |
| DV-62304-19 | 5.6, 5.7 | Run regression and system tests against all requirements on each claimed deployment configuration. Evaluate test adequacy and resolve anomalies. | Traceability matrix, executed reports, coverage rationale and problem records. |
| DV-62304-20 | 5.7 | Preserve repeatable test inputs, expected outputs, tolerances, artifact identifiers, tools, dates and executors. | Dataset identifiers and hashes, ground truth, environment capture and signed test reports. |

## Release and maintenance

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62304-21 | 5.8 | Confirm planned activities and verification are complete. Evaluate every residual anomaly before software release. | Software release review, anomaly list, safety impact and authorized disposition. |
| DV-62304-22 | 5.8 | Identify the release and preserve its sources, dependencies, build procedure, configuration, artifacts and delivery controls. | Product/component version mapping, SBOM, build provenance, artifact hashes and retention/distribution records. |
| DV-62304-23 | 6.1 | Establish support responsibility after the archive decision, including feedback, supplier updates, security patches and end of support. | Approved maintenance plan, service contacts and review cadence. |
| DV-62304-24 | 6.2 | Assess reported problems and proposed changes for effects on patients, users, connected systems and deployed versions. | Problem/change records, risk analysis, approval and notification decisions. |
| DV-62304-25 | 6.3 | Reapply affected development, verification and release activities when changing the viewer or a dependency. | Update impact assessment, regression selection, retest results and redistribution approval. |

## Risk, configuration and problem resolution

| ID | Clause group | Review check for dicom_viewer | Evidence required |
|---|---|---|---|
| DV-62304-26 | 7.1 | Identify software contributions to hazardous situations, including wrong patient/series, incorrect geometry, stale frames and SOUP failures. | Risk file linking causes, items and known supplier anomalies. |
| DV-62304-27 | 7.2, 7.3 | Specify, implement and verify each software risk control; assess new risks introduced by the control. | Bidirectional risk-to-requirement-to-implementation-to-test traceability. |
| DV-62304-28 | 7.4 | Reassess safety after dependency, algorithm, transport, authentication, database or GPU changes. | Before/after behavior assessment and updated risk-control verification. |
| DV-62304-29 | 8.1 | Uniquely identify all configuration items and SOUP versions. Freeze all kcenon SHAs and third-party package versions. | Controlled lock/provenance records; distinguish released packages from unreleased source snapshots. |
| DV-62304-30 | 8.2, 8.3 | Use approved, traceable changes and recoverable configuration history. Ensure setup scripts and CI consume the same baseline. | Change approvals, build records, cache invalidation rules and configuration audits. |
| DV-62304-31 | 9.1 through 9.5 | Record, investigate, communicate and resolve problems through controlled changes. Capture reasons for taking no action. | Problem reports with severity, affected versions, root cause, risk review and resolution history. |
| DV-62304-32 | 9.6 through 9.8 | Review defect trends and verify resolution and regression results before closure. | Trend reviews and repeatable closure tests with version, configuration, executor and date. |

## Evidence starting points

- [Requirements](../SRS.md), [design](../SDS.md), [server](../../server), [client](../../client).
- [PACS services](../../src/services/pacs), [CMake](../../CMakeLists.txt), [CI](../../.github/workflows/ci.yml), [tests](../../tests).
- [Risk checklist](iso-14971-checklist.md), [security checklist](iec-81001-5-1-checklist.md), [MFDS GMP checklist](mfds-digital-gmp-checklist.md).
