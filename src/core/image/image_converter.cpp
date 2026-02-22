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

#include "core/image_converter.hpp"

#include <itkImageToVTKImageFilter.h>
#include <itkVTKImageToImageFilter.h>
#include <itkImageRegionIterator.h>

namespace dicom_viewer::core {

vtkSmartPointer<vtkImageData>
ImageConverter::itkToVtk(CTImageType::Pointer itkImage)
{
    using ConnectorType = itk::ImageToVTKImageFilter<CTImageType>;
    auto connector = ConnectorType::New();
    connector->SetInput(itkImage);
    connector->Update();

    auto vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->DeepCopy(connector->GetOutput());
    return vtkImage;
}

vtkSmartPointer<vtkImageData>
ImageConverter::itkToVtk(MRImageType::Pointer itkImage)
{
    using ConnectorType = itk::ImageToVTKImageFilter<MRImageType>;
    auto connector = ConnectorType::New();
    connector->SetInput(itkImage);
    connector->Update();

    auto vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->DeepCopy(connector->GetOutput());
    return vtkImage;
}

vtkSmartPointer<vtkImageData>
ImageConverter::itkToVtk(FloatImageType::Pointer itkImage)
{
    using ConnectorType = itk::ImageToVTKImageFilter<FloatImageType>;
    auto connector = ConnectorType::New();
    connector->SetInput(itkImage);
    connector->Update();

    auto vtkImage = vtkSmartPointer<vtkImageData>::New();
    vtkImage->DeepCopy(connector->GetOutput());
    return vtkImage;
}

ImageConverter::CTImageType::Pointer
ImageConverter::vtkToItkCT(vtkSmartPointer<vtkImageData> vtkImage)
{
    using ConnectorType = itk::VTKImageToImageFilter<CTImageType>;
    auto connector = ConnectorType::New();
    connector->SetInput(vtkImage);
    connector->Update();

    auto output = CTImageType::New();
    output->Graft(connector->GetOutput());
    return output;
}

ImageConverter::FloatImageType::Pointer
ImageConverter::vtkToItkFloat(vtkSmartPointer<vtkImageData> vtkImage)
{
    using ConnectorType = itk::VTKImageToImageFilter<FloatImageType>;
    auto connector = ConnectorType::New();
    connector->SetInput(vtkImage);
    connector->Update();

    auto output = FloatImageType::New();
    output->Graft(connector->GetOutput());
    return output;
}

void ImageConverter::applyHUConversion(
    CTImageType::Pointer image,
    double slope,
    double intercept)
{
    using IteratorType = itk::ImageRegionIterator<CTImageType>;
    IteratorType it(image, image->GetLargestPossibleRegion());

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        double value = it.Get() * slope + intercept;
        it.Set(static_cast<short>(value));
    }
}

} // namespace dicom_viewer::core
