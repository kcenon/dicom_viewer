#!/bin/bash
# Download sample DICOM data for testing
# Usage: ./download_sample_data.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="${SCRIPT_DIR}"

echo "=== DICOM Sample Data Downloader ==="
echo "Download directory: ${DATA_DIR}"
echo ""

# Create directories
mkdir -p "${DATA_DIR}/ct_sample"
mkdir -p "${DATA_DIR}/mr_sample"

# Option 1: Download from TCIA (requires NBIA Data Retriever)
download_tcia_sample() {
    echo "TCIA samples require NBIA Data Retriever."
    echo "Visit: https://wiki.cancerimagingarchive.net/display/NBIA/NBIA+Data+Retriever"
    echo ""
}

# Option 2: Instructions for manual download
show_manual_instructions() {
    cat << 'EOF'
=== Manual Download Instructions ===

1. OsiriX DICOM Library (Recommended for quick tests):
   URL: https://www.osirix-viewer.com/resources/dicom-image-library/
   - Download any CT dataset (e.g., "BRAINIX" or "COLONIX")
   - Extract to: tests/data/ct_sample/

2. Aliza Medical Imaging Datasets:
   URL: https://www.aliza-dicom-viewer.com/download/datasets
   - Various vendor-specific samples available
   - Good for testing Transfer Syntax compatibility

3. Medimodel Free Samples:
   URL: https://medimodel.com/sample-dicom-files/
   - Anonymized CT and MRI scans
   - Good for basic functionality testing

=== Expected Directory Structure ===

tests/data/
├── ct_sample/
│   ├── series_001/
│   │   ├── IM001.dcm
│   │   ├── IM002.dcm
│   │   └── ...
│   └── series_002/
│       └── ...
└── mr_sample/
    └── series_001/
        └── ...

EOF
}

# Option 3: Create mock DICOM files for unit tests (no real imaging data)
create_mock_dicom_info() {
    cat << 'EOF'
=== Mock DICOM for Unit Tests ===

For unit tests that don't require real image data,
consider using ITK's built-in test images or creating
synthetic DICOM files with GDCM.

The current unit tests in series_assembly_test.cpp
use in-memory SliceInfo objects, so no actual DICOM
files are required for basic testing.

For integration tests with real files:
1. Download sample data using instructions above
2. Set environment variable: DICOM_TEST_DATA_DIR=/path/to/data
3. Integration tests will automatically use this path

EOF
}

# Main menu
echo "Choose an option:"
echo "1) Show manual download instructions"
echo "2) Show TCIA download info"
echo "3) Show mock DICOM info for unit tests"
echo ""
read -p "Enter choice [1-3]: " choice

case $choice in
    1) show_manual_instructions ;;
    2) download_tcia_sample ;;
    3) create_mock_dicom_info ;;
    *) echo "Invalid choice"; exit 1 ;;
esac

echo ""
echo "=== Done ==="
