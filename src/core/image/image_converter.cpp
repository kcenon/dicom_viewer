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
