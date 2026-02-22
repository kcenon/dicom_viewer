// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file image_converter.hpp
 * @brief Conversion between ITK and VTK image representations
 * @details Bridges the ITK and VTK image processing ecosystems by converting
 *          itk::Image to vtkImageData and vice versa. Handles pixel type
 *          mapping and coordinate system alignment between ITK LPS and
 *          VTK coordinate conventions.
 *
 * @author kcenon
 * @since 1.0.0
 */

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
