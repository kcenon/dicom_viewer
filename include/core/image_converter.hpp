#pragma once

#include <memory>

#include <itkImage.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>

namespace dicom_viewer::core {

/**
 * @brief Image converter between ITK and VTK formats
 *
 * Provides utilities for converting between ITK images and VTK image data,
 * with proper handling of coordinate systems and spacing.
 *
 * @trace SRS-IF-001, SRS-IF-002, SRS-ARCH-004
 */
class ImageConverter {
public:
    using CTImageType = itk::Image<short, 3>;
    using MRImageType = itk::Image<unsigned short, 3>;
    using FloatImageType = itk::Image<float, 3>;
    using MaskImageType = itk::Image<unsigned char, 3>;

    /**
     * @brief Convert ITK CT image to VTK image data
     * @param itkImage ITK image pointer
     * @return VTK image data
     */
    static vtkSmartPointer<vtkImageData>
    itkToVtk(CTImageType::Pointer itkImage);

    /**
     * @brief Convert ITK MR image to VTK image data
     * @param itkImage ITK image pointer
     * @return VTK image data
     */
    static vtkSmartPointer<vtkImageData>
    itkToVtk(MRImageType::Pointer itkImage);

    /**
     * @brief Convert ITK float image to VTK image data
     * @param itkImage ITK image pointer
     * @return VTK image data
     */
    static vtkSmartPointer<vtkImageData>
    itkToVtk(FloatImageType::Pointer itkImage);

    /**
     * @brief Convert VTK image data to ITK CT image
     * @param vtkImage VTK image data
     * @return ITK image pointer
     */
    static CTImageType::Pointer
    vtkToItkCT(vtkSmartPointer<vtkImageData> vtkImage);

    /**
     * @brief Convert VTK image data to ITK float image
     * @param vtkImage VTK image data
     * @return ITK image pointer
     */
    static FloatImageType::Pointer
    vtkToItkFloat(vtkSmartPointer<vtkImageData> vtkImage);

    /**
     * @brief Apply Hounsfield Unit conversion to image
     * @param image ITK image to convert in place
     * @param slope Rescale slope from DICOM
     * @param intercept Rescale intercept from DICOM
     * @trace SRS-FR-004
     */
    static void applyHUConversion(
        CTImageType::Pointer image,
        double slope,
        double intercept
    );
};

} // namespace dicom_viewer::core
