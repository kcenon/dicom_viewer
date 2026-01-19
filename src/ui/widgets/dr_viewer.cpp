#include "ui/dr_viewer.hpp"
#include "core/dicom_loader.hpp"

#include <array>
#include <unordered_map>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QWheelEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageSlice.h>
#include <vtkImageProperty.h>
#include <vtkCamera.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkLeaderActor2D.h>
#include <vtkActor2D.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkRegularPolygonSource.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkScalarBarActor.h>
#include <vtkLookupTable.h>
#include <vtkCoordinate.h>
#include <vtkProperty2D.h>

namespace dicom_viewer::ui {

namespace {

// Standard DR presets
const std::vector<DRPreset> STANDARD_PRESETS = {
    {"Chest", 2000, 0, "Standard chest X-ray"},
    {"Bone", 2500, 500, "Bone visualization"},
    {"Soft Tissue", 400, 40, "Soft tissue detail"},
    {"Lung", 1500, -600, "Lung parenchyma"},
    {"Mediastinum", 500, 50, "Mediastinal structures"},
    {"Abdomen", 400, 50, "Abdominal soft tissue"},
    {"Pediatric", 1500, 0, "Pediatric chest"}
};

// DICOM orientation character to display mapping
QString getOrientationLabel(char orientation) {
    switch (orientation) {
        case 'L': return "L";  // Left
        case 'R': return "R";  // Right
        case 'A': return "A";  // Anterior
        case 'P': return "P";  // Posterior
        case 'H': return "S";  // Head (Superior)
        case 'F': return "I";  // Feet (Inferior)
        case 'S': return "S";  // Superior
        case 'I': return "I";  // Inferior
        default: return "";
    }
}

} // anonymous namespace

class DRViewer::Impl {
public:
    QVTKOpenGLNativeWidget* vtkWidget = nullptr;
    QVTKOpenGLNativeWidget* comparisonWidget = nullptr;
    QSplitter* splitter = nullptr;

    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkInteractorStyleImage> imageStyle;
    vtkSmartPointer<vtkImageSliceMapper> sliceMapper;
    vtkSmartPointer<vtkImageSlice> imageSlice;
    vtkSmartPointer<vtkImageProperty> imageProperty;
    vtkSmartPointer<vtkImageData> imageData;

    // Comparison view
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> comparisonRenderWindow;
    vtkSmartPointer<vtkRenderer> comparisonRenderer;
    vtkSmartPointer<vtkImageSliceMapper> comparisonSliceMapper;
    vtkSmartPointer<vtkImageSlice> comparisonImageSlice;
    vtkSmartPointer<vtkImageData> comparisonImageData;

    // Orientation markers (text actors)
    vtkSmartPointer<vtkTextActor> leftMarker;
    vtkSmartPointer<vtkTextActor> rightMarker;
    vtkSmartPointer<vtkTextActor> topMarker;
    vtkSmartPointer<vtkTextActor> bottomMarker;

    // Patient/Study info overlay
    vtkSmartPointer<vtkTextActor> patientInfoActor;
    vtkSmartPointer<vtkTextActor> studyInfoActor;

    // Scale bar
    vtkSmartPointer<vtkTextActor> scaleBarText;
    vtkSmartPointer<vtkActor2D> scaleBarLine;

    // Annotations
    std::vector<DRAnnotation> annotations;
    std::unordered_map<int, vtkSmartPointer<vtkTextActor>> textActors;
    std::unordered_map<int, vtkSmartPointer<vtkLeaderActor2D>> arrowActors;
    std::unordered_map<int, vtkSmartPointer<vtkActor2D>> markerActors;
    int nextAnnotationId = 1;

    // State
    double windowWidth = 2000.0;
    double windowCenter = 0.0;
    double zoomLevel = 1.0;
    double pixelSpacing = 1.0;
    bool calibrated = false;

    // Display options
    bool showOrientationMarkers = true;
    bool showPatientInfo = true;
    bool showStudyInfo = true;
    bool showScaleBar = true;

    // Comparison options
    ComparisonLayout comparisonLayout = ComparisonLayout::SideBySide;
    bool linkZoomPan = true;

    // Patient/Study metadata
    QString patientName;
    QString patientId;
    QString studyDate;
    QString modality;
    QString studyDescription;
    QString laterality;
    QString viewPosition;
    std::array<char, 2> rowOrientation = {'L', 'R'};
    std::array<char, 2> colOrientation = {'H', 'F'};

    Impl() {
        renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        renderer = vtkSmartPointer<vtkRenderer>::New();
        imageStyle = vtkSmartPointer<vtkInteractorStyleImage>::New();
        sliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        imageSlice = vtkSmartPointer<vtkImageSlice>::New();
        imageProperty = vtkSmartPointer<vtkImageProperty>::New();

        renderer->SetBackground(0.0, 0.0, 0.0);
        renderWindow->AddRenderer(renderer);

        imageProperty->SetColorWindow(windowWidth);
        imageProperty->SetColorLevel(windowCenter);
        imageProperty->SetInterpolationTypeToLinear();

        imageSlice->SetMapper(sliceMapper);
        imageSlice->SetProperty(imageProperty);

        setupOrientationMarkers();
        setupInfoOverlays();
        setupScaleBar();

        // Comparison view
        comparisonRenderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        comparisonRenderer = vtkSmartPointer<vtkRenderer>::New();
        comparisonSliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        comparisonImageSlice = vtkSmartPointer<vtkImageSlice>::New();

        comparisonRenderer->SetBackground(0.0, 0.0, 0.0);
        comparisonRenderWindow->AddRenderer(comparisonRenderer);

        auto comparisonProperty = vtkSmartPointer<vtkImageProperty>::New();
        comparisonProperty->SetColorWindow(windowWidth);
        comparisonProperty->SetColorLevel(windowCenter);
        comparisonProperty->SetInterpolationTypeToLinear();

        comparisonImageSlice->SetMapper(comparisonSliceMapper);
        comparisonImageSlice->SetProperty(comparisonProperty);
    }

    void setupOrientationMarkers() {
        auto setupMarker = [](vtkSmartPointer<vtkTextActor>& actor, const char* text) {
            actor = vtkSmartPointer<vtkTextActor>::New();
            actor->SetInput(text);
            actor->GetTextProperty()->SetFontSize(24);
            actor->GetTextProperty()->SetColor(1.0, 1.0, 0.0);  // Yellow
            actor->GetTextProperty()->SetFontFamilyToArial();
            actor->GetTextProperty()->BoldOn();
            actor->SetVisibility(true);
        };

        setupMarker(leftMarker, "L");
        setupMarker(rightMarker, "R");
        setupMarker(topMarker, "S");
        setupMarker(bottomMarker, "I");

        // Position markers (normalized viewport coordinates)
        leftMarker->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        leftMarker->GetPositionCoordinate()->SetValue(0.02, 0.5);

        rightMarker->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        rightMarker->GetPositionCoordinate()->SetValue(0.95, 0.5);

        topMarker->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        topMarker->GetPositionCoordinate()->SetValue(0.5, 0.95);

        bottomMarker->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        bottomMarker->GetPositionCoordinate()->SetValue(0.5, 0.02);
    }

    void setupInfoOverlays() {
        // Patient info (top-left)
        patientInfoActor = vtkSmartPointer<vtkTextActor>::New();
        patientInfoActor->GetTextProperty()->SetFontSize(14);
        patientInfoActor->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
        patientInfoActor->GetTextProperty()->SetFontFamilyToArial();
        patientInfoActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        patientInfoActor->GetPositionCoordinate()->SetValue(0.02, 0.88);

        // Study info (bottom-left)
        studyInfoActor = vtkSmartPointer<vtkTextActor>::New();
        studyInfoActor->GetTextProperty()->SetFontSize(12);
        studyInfoActor->GetTextProperty()->SetColor(0.8, 0.8, 0.8);
        studyInfoActor->GetTextProperty()->SetFontFamilyToArial();
        studyInfoActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        studyInfoActor->GetPositionCoordinate()->SetValue(0.02, 0.02);
    }

    void setupScaleBar() {
        scaleBarText = vtkSmartPointer<vtkTextActor>::New();
        scaleBarText->GetTextProperty()->SetFontSize(12);
        scaleBarText->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
        scaleBarText->GetTextProperty()->SetFontFamilyToArial();
        scaleBarText->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        scaleBarText->GetPositionCoordinate()->SetValue(0.85, 0.02);
    }

    void addActorsToRenderer() {
        renderer->AddActor(imageSlice);

        if (showOrientationMarkers) {
            renderer->AddActor2D(leftMarker);
            renderer->AddActor2D(rightMarker);
            renderer->AddActor2D(topMarker);
            renderer->AddActor2D(bottomMarker);
        }

        if (showPatientInfo) {
            renderer->AddActor2D(patientInfoActor);
        }

        if (showStudyInfo) {
            renderer->AddActor2D(studyInfoActor);
        }

        if (showScaleBar) {
            renderer->AddActor2D(scaleBarText);
        }

        // Add annotation actors
        for (const auto& [id, actor] : textActors) {
            renderer->AddActor2D(actor);
        }
        for (const auto& [id, actor] : arrowActors) {
            renderer->AddActor2D(actor);
        }
        for (const auto& [id, actor] : markerActors) {
            renderer->AddActor2D(actor);
        }
    }

    void updateOrientationMarkers() {
        leftMarker->SetInput(getOrientationLabel(rowOrientation[0]).toStdString().c_str());
        rightMarker->SetInput(getOrientationLabel(rowOrientation[1]).toStdString().c_str());
        topMarker->SetInput(getOrientationLabel(colOrientation[0]).toStdString().c_str());
        bottomMarker->SetInput(getOrientationLabel(colOrientation[1]).toStdString().c_str());

        leftMarker->SetVisibility(showOrientationMarkers);
        rightMarker->SetVisibility(showOrientationMarkers);
        topMarker->SetVisibility(showOrientationMarkers);
        bottomMarker->SetVisibility(showOrientationMarkers);
    }

    void updatePatientInfo() {
        QString info;
        if (!patientName.isEmpty()) {
            info += patientName + "\n";
        }
        if (!patientId.isEmpty()) {
            info += "ID: " + patientId;
        }
        patientInfoActor->SetInput(info.toStdString().c_str());
        patientInfoActor->SetVisibility(showPatientInfo && !info.isEmpty());
    }

    void updateStudyInfo() {
        QString info;
        if (!studyDate.isEmpty()) {
            info += "Date: " + studyDate + "\n";
        }
        if (!modality.isEmpty()) {
            info += modality;
            if (!viewPosition.isEmpty()) {
                info += " - " + viewPosition;
            }
            if (!laterality.isEmpty()) {
                info += " (" + laterality + ")";
            }
        }
        if (!studyDescription.isEmpty()) {
            info += "\n" + studyDescription;
        }
        studyInfoActor->SetInput(info.toStdString().c_str());
        studyInfoActor->SetVisibility(showStudyInfo && !info.isEmpty());
    }

    void updateScaleBar() {
        if (calibrated && pixelSpacing > 0) {
            double scaleLength = 100.0;  // 100mm = 10cm
            QString text = QString("10 cm");
            scaleBarText->SetInput(text.toStdString().c_str());
        } else {
            scaleBarText->SetInput("Not calibrated");
        }
        scaleBarText->SetVisibility(showScaleBar);
    }

    void render() {
        if (vtkWidget && vtkWidget->renderWindow()) {
            vtkWidget->renderWindow()->Render();
        }
    }

    void renderComparison() {
        if (comparisonWidget && comparisonWidget->renderWindow()) {
            comparisonWidget->renderWindow()->Render();
        }
    }
};

DRViewer::DRViewer(QWidget* parent)
    : QWidget(parent)
    , impl_(std::make_unique<Impl>())
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    impl_->splitter = new QSplitter(Qt::Horizontal, this);

    // Main VTK widget
    impl_->vtkWidget = new QVTKOpenGLNativeWidget(impl_->splitter);
    impl_->vtkWidget->setRenderWindow(impl_->renderWindow);
    impl_->splitter->addWidget(impl_->vtkWidget);

    // Comparison widget (hidden by default)
    impl_->comparisonWidget = new QVTKOpenGLNativeWidget(impl_->splitter);
    impl_->comparisonWidget->setRenderWindow(impl_->comparisonRenderWindow);
    impl_->comparisonWidget->hide();
    impl_->splitter->addWidget(impl_->comparisonWidget);

    mainLayout->addWidget(impl_->splitter);

    // Setup interactor
    auto interactor = impl_->renderWindow->GetInteractor();
    interactor->SetInteractorStyle(impl_->imageStyle);

    impl_->addActorsToRenderer();
    setLayout(mainLayout);
}

DRViewer::~DRViewer() = default;

void DRViewer::setImage(vtkSmartPointer<vtkImageData> image)
{
    impl_->imageData = image;

    if (image) {
        impl_->sliceMapper->SetInputData(image);
        impl_->sliceMapper->SetSliceNumber(0);  // DR/CR is typically single slice

        // Get pixel spacing from image
        double* spacing = image->GetSpacing();
        if (spacing[0] > 0 && spacing[0] != 1.0) {
            impl_->pixelSpacing = spacing[0];
            impl_->calibrated = true;
        }

        impl_->renderer->ResetCamera();
        impl_->updateScaleBar();
    }

    impl_->render();
}

void DRViewer::setDicomMetadata(const core::DicomMetadata& metadata)
{
    // Extract patient info
    impl_->patientName = QString::fromStdString(metadata.patientName);
    impl_->patientId = QString::fromStdString(metadata.patientId);

    // Extract study info
    impl_->studyDate = QString::fromStdString(metadata.studyDate);
    impl_->modality = QString::fromStdString(metadata.modality);
    impl_->studyDescription = QString::fromStdString(metadata.studyDescription);

    // Extract pixel spacing for calibration
    if (metadata.pixelSpacingX > 0 && metadata.pixelSpacingX != 1.0) {
        impl_->pixelSpacing = metadata.pixelSpacingX;
        impl_->calibrated = true;
    }

    // Update display
    impl_->updateOrientationMarkers();
    impl_->updatePatientInfo();
    impl_->updateStudyInfo();
    impl_->updateScaleBar();
    impl_->render();
}

vtkSmartPointer<vtkImageData> DRViewer::getImage() const
{
    return impl_->imageData;
}

void DRViewer::setShowOrientationMarkers(bool show)
{
    impl_->showOrientationMarkers = show;
    impl_->updateOrientationMarkers();
    impl_->render();
}

void DRViewer::setShowPatientInfo(bool show)
{
    impl_->showPatientInfo = show;
    impl_->patientInfoActor->SetVisibility(show);
    impl_->render();
}

void DRViewer::setShowStudyInfo(bool show)
{
    impl_->showStudyInfo = show;
    impl_->studyInfoActor->SetVisibility(show);
    impl_->render();
}

void DRViewer::setShowScaleBar(bool show)
{
    impl_->showScaleBar = show;
    impl_->scaleBarText->SetVisibility(show);
    impl_->render();
}

void DRViewer::setWindowLevel(double window, double level)
{
    impl_->windowWidth = window;
    impl_->windowCenter = level;
    impl_->imageProperty->SetColorWindow(window);
    impl_->imageProperty->SetColorLevel(level);

    // Sync comparison view if linked
    if (impl_->comparisonImageData && impl_->linkZoomPan) {
        impl_->comparisonImageSlice->GetProperty()->SetColorWindow(window);
        impl_->comparisonImageSlice->GetProperty()->SetColorLevel(level);
        impl_->renderComparison();
    }

    impl_->render();
    emit windowLevelChanged(window, level);
}

std::pair<double, double> DRViewer::getWindowLevel() const
{
    return {impl_->windowWidth, impl_->windowCenter};
}

void DRViewer::applyPreset(const QString& presetName)
{
    for (const auto& preset : STANDARD_PRESETS) {
        if (preset.name == presetName) {
            setWindowLevel(preset.windowWidth, preset.windowCenter);
            return;
        }
    }
}

std::vector<QString> DRViewer::getAvailablePresets() const
{
    std::vector<QString> names;
    names.reserve(STANDARD_PRESETS.size());
    for (const auto& preset : STANDARD_PRESETS) {
        names.push_back(preset.name);
    }
    return names;
}

std::optional<DRPreset> DRViewer::getPreset(const QString& name) const
{
    for (const auto& preset : STANDARD_PRESETS) {
        if (preset.name == name) {
            return preset;
        }
    }
    return std::nullopt;
}

void DRViewer::setZoomLevel(double zoom)
{
    if (zoom <= 0) return;

    impl_->zoomLevel = zoom;

    auto camera = impl_->renderer->GetActiveCamera();
    if (camera) {
        camera->SetParallelScale(camera->GetParallelScale() / zoom);
        impl_->render();
    }

    if (impl_->comparisonImageData && impl_->linkZoomPan) {
        auto compCamera = impl_->comparisonRenderer->GetActiveCamera();
        if (compCamera) {
            compCamera->SetParallelScale(compCamera->GetParallelScale() / zoom);
            impl_->renderComparison();
        }
    }

    emit zoomLevelChanged(zoom);
}

double DRViewer::getZoomLevel() const
{
    return impl_->zoomLevel;
}

void DRViewer::fitToWindow()
{
    impl_->renderer->ResetCamera();
    impl_->zoomLevel = 1.0;
    impl_->render();

    if (impl_->comparisonImageData) {
        impl_->comparisonRenderer->ResetCamera();
        impl_->renderComparison();
    }

    emit zoomLevelChanged(impl_->zoomLevel);
}

void DRViewer::actualSize()
{
    if (!impl_->imageData) return;

    // Calculate zoom for 1:1 pixel display
    int* dims = impl_->imageData->GetDimensions();
    int viewportWidth = impl_->vtkWidget->width();
    int viewportHeight = impl_->vtkWidget->height();

    double zoomX = static_cast<double>(viewportWidth) / dims[0];
    double zoomY = static_cast<double>(viewportHeight) / dims[1];
    double zoom = std::min(zoomX, zoomY);

    // Reset camera first
    impl_->renderer->ResetCamera();

    // Adjust for actual size
    auto camera = impl_->renderer->GetActiveCamera();
    if (camera) {
        double currentScale = camera->GetParallelScale();
        camera->SetParallelScale(currentScale / zoom);
    }

    impl_->zoomLevel = zoom;
    impl_->render();
    emit zoomLevelChanged(impl_->zoomLevel);
}

void DRViewer::resetView()
{
    fitToWindow();
    applyPreset(impl_->modality == "CR" || impl_->modality == "DX" ? "Chest" : "Chest");
}

void DRViewer::setPixelSpacing(double spacingMm)
{
    if (spacingMm > 0) {
        impl_->pixelSpacing = spacingMm;
        impl_->calibrated = true;
        impl_->updateScaleBar();
        impl_->render();
    }
}

double DRViewer::getPixelSpacing() const
{
    return impl_->pixelSpacing;
}

bool DRViewer::isCalibrated() const
{
    return impl_->calibrated;
}

int DRViewer::addTextAnnotation(const QPointF& position, const QString& text)
{
    int id = impl_->nextAnnotationId++;

    DRAnnotation annotation;
    annotation.id = id;
    annotation.type = DRAnnotationType::Text;
    annotation.position = position;
    annotation.text = text;
    impl_->annotations.push_back(annotation);

    // Create VTK text actor
    auto textActor = vtkSmartPointer<vtkTextActor>::New();
    textActor->SetInput(text.toStdString().c_str());
    textActor->GetTextProperty()->SetFontSize(14);
    textActor->GetTextProperty()->SetColor(1.0, 1.0, 0.0);  // Yellow
    textActor->GetTextProperty()->SetFontFamilyToArial();
    textActor->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    textActor->GetPositionCoordinate()->SetValue(position.x(), position.y(), 0);

    impl_->textActors[id] = textActor;
    impl_->renderer->AddActor2D(textActor);
    impl_->render();

    emit annotationAdded(id);
    return id;
}

int DRViewer::addArrowAnnotation(const QPointF& start, const QPointF& end)
{
    int id = impl_->nextAnnotationId++;

    DRAnnotation annotation;
    annotation.id = id;
    annotation.type = DRAnnotationType::Arrow;
    annotation.position = start;
    annotation.endPosition = end;
    impl_->annotations.push_back(annotation);

    // Create VTK leader actor for arrow
    auto arrowActor = vtkSmartPointer<vtkLeaderActor2D>::New();
    arrowActor->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    arrowActor->GetPositionCoordinate()->SetValue(start.x(), start.y(), 0);
    arrowActor->GetPosition2Coordinate()->SetCoordinateSystemToWorld();
    arrowActor->GetPosition2Coordinate()->SetValue(end.x(), end.y(), 0);
    arrowActor->SetArrowStyleToFilled();
    arrowActor->SetArrowPlacementToPoint2();
    arrowActor->GetProperty()->SetColor(1.0, 1.0, 0.0);  // Yellow

    impl_->arrowActors[id] = arrowActor;
    impl_->renderer->AddActor2D(arrowActor);
    impl_->render();

    emit annotationAdded(id);
    return id;
}

int DRViewer::addMarker(const QPointF& position, int number)
{
    int id = impl_->nextAnnotationId++;

    DRAnnotation annotation;
    annotation.id = id;
    annotation.type = DRAnnotationType::Marker;
    annotation.position = position;
    annotation.markerNumber = number;
    impl_->annotations.push_back(annotation);

    // Create text actor for numbered marker
    auto markerActor = vtkSmartPointer<vtkTextActor>::New();
    markerActor->SetInput(std::to_string(number).c_str());
    markerActor->GetTextProperty()->SetFontSize(16);
    markerActor->GetTextProperty()->SetColor(0.0, 1.0, 0.0);  // Green
    markerActor->GetTextProperty()->SetFontFamilyToArial();
    markerActor->GetTextProperty()->BoldOn();
    markerActor->GetTextProperty()->SetBackgroundColor(0.0, 0.0, 0.0);
    markerActor->GetTextProperty()->SetBackgroundOpacity(0.5);
    markerActor->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    markerActor->GetPositionCoordinate()->SetValue(position.x(), position.y(), 0);

    // Store as text actor (numbered markers are rendered as text)
    auto markerActor2D = vtkSmartPointer<vtkActor2D>::New();
    impl_->textActors[id] = markerActor;
    impl_->renderer->AddActor2D(markerActor);
    impl_->render();

    emit annotationAdded(id);
    return id;
}

std::vector<DRAnnotation> DRViewer::getAnnotations() const
{
    return impl_->annotations;
}

void DRViewer::removeAnnotation(int id)
{
    // Remove from annotations vector
    impl_->annotations.erase(
        std::remove_if(impl_->annotations.begin(), impl_->annotations.end(),
            [id](const DRAnnotation& ann) { return ann.id == id; }),
        impl_->annotations.end());

    // Remove VTK actors
    if (auto it = impl_->textActors.find(id); it != impl_->textActors.end()) {
        impl_->renderer->RemoveActor2D(it->second);
        impl_->textActors.erase(it);
    }
    if (auto it = impl_->arrowActors.find(id); it != impl_->arrowActors.end()) {
        impl_->renderer->RemoveActor2D(it->second);
        impl_->arrowActors.erase(it);
    }
    if (auto it = impl_->markerActors.find(id); it != impl_->markerActors.end()) {
        impl_->renderer->RemoveActor2D(it->second);
        impl_->markerActors.erase(it);
    }

    impl_->render();
    emit annotationRemoved(id);
}

void DRViewer::clearAnnotations()
{
    for (const auto& [id, actor] : impl_->textActors) {
        impl_->renderer->RemoveActor2D(actor);
    }
    for (const auto& [id, actor] : impl_->arrowActors) {
        impl_->renderer->RemoveActor2D(actor);
    }
    for (const auto& [id, actor] : impl_->markerActors) {
        impl_->renderer->RemoveActor2D(actor);
    }

    impl_->textActors.clear();
    impl_->arrowActors.clear();
    impl_->markerActors.clear();
    impl_->annotations.clear();

    impl_->render();
}

bool DRViewer::saveAnnotations(const QString& filePath) const
{
    QJsonArray annotationsArray;
    for (const auto& ann : impl_->annotations) {
        QJsonObject obj;
        obj["id"] = ann.id;
        obj["type"] = static_cast<int>(ann.type);
        obj["posX"] = ann.position.x();
        obj["posY"] = ann.position.y();
        obj["endX"] = ann.endPosition.x();
        obj["endY"] = ann.endPosition.y();
        obj["text"] = ann.text;
        obj["markerNumber"] = ann.markerNumber;
        obj["visible"] = ann.visible;
        annotationsArray.append(obj);
    }

    QJsonObject root;
    root["version"] = "1.0.0";
    root["annotations"] = annotationsArray;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson());
    return true;
}

bool DRViewer::loadAnnotations(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    clearAnnotations();

    QJsonObject root = doc.object();
    QJsonArray annotationsArray = root["annotations"].toArray();

    for (const QJsonValue& val : annotationsArray) {
        QJsonObject obj = val.toObject();
        DRAnnotationType type = static_cast<DRAnnotationType>(obj["type"].toInt());
        QPointF pos(obj["posX"].toDouble(), obj["posY"].toDouble());

        switch (type) {
            case DRAnnotationType::Text:
                addTextAnnotation(pos, obj["text"].toString());
                break;
            case DRAnnotationType::Arrow: {
                QPointF endPos(obj["endX"].toDouble(), obj["endY"].toDouble());
                addArrowAnnotation(pos, endPos);
                break;
            }
            case DRAnnotationType::Marker:
                addMarker(pos, obj["markerNumber"].toInt());
                break;
        }
    }

    return true;
}

void DRViewer::setComparisonImage(vtkSmartPointer<vtkImageData> priorImage)
{
    impl_->comparisonImageData = priorImage;

    if (priorImage) {
        impl_->comparisonSliceMapper->SetInputData(priorImage);
        impl_->comparisonSliceMapper->SetSliceNumber(0);
        impl_->comparisonRenderer->AddActor(impl_->comparisonImageSlice);
        impl_->comparisonRenderer->ResetCamera();

        // Sync window/level
        impl_->comparisonImageSlice->GetProperty()->SetColorWindow(impl_->windowWidth);
        impl_->comparisonImageSlice->GetProperty()->SetColorLevel(impl_->windowCenter);

        impl_->comparisonWidget->show();
    } else {
        impl_->comparisonWidget->hide();
    }

    impl_->renderComparison();
}

void DRViewer::setComparisonLayout(ComparisonLayout layout)
{
    impl_->comparisonLayout = layout;

    switch (layout) {
        case ComparisonLayout::SideBySide:
            impl_->splitter->setOrientation(Qt::Horizontal);
            break;
        case ComparisonLayout::TopBottom:
            impl_->splitter->setOrientation(Qt::Vertical);
            break;
        case ComparisonLayout::Overlay:
            // Overlay mode requires different implementation
            break;
    }
}

void DRViewer::enableLinkZoomPan(bool enable)
{
    impl_->linkZoomPan = enable;
}

bool DRViewer::isComparisonActive() const
{
    return impl_->comparisonImageData != nullptr;
}

void DRViewer::clearComparison()
{
    impl_->comparisonImageData = nullptr;
    impl_->comparisonRenderer->RemoveActor(impl_->comparisonImageSlice);
    impl_->comparisonWidget->hide();
}

bool DRViewer::captureScreenshot(const QString& filePath)
{
    auto windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
    windowToImage->SetInput(impl_->renderWindow);
    windowToImage->SetScale(1);
    windowToImage->SetInputBufferTypeToRGBA();
    windowToImage->ReadFrontBufferOff();
    windowToImage->Update();

    auto writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(filePath.toStdString().c_str());
    writer->SetInputConnection(windowToImage->GetOutputPort());
    writer->Write();

    return true;
}

void DRViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    impl_->render();
}

void DRViewer::wheelEvent(QWheelEvent* event)
{
    // Zoom with mouse wheel
    double delta = event->angleDelta().y() > 0 ? 1.1 : 0.9;
    double newZoom = impl_->zoomLevel * delta;

    if (newZoom >= 0.1 && newZoom <= 10.0) {
        auto camera = impl_->renderer->GetActiveCamera();
        if (camera) {
            camera->SetParallelScale(camera->GetParallelScale() / delta);
            impl_->zoomLevel = newZoom;
            impl_->render();

            if (impl_->comparisonImageData && impl_->linkZoomPan) {
                auto compCamera = impl_->comparisonRenderer->GetActiveCamera();
                if (compCamera) {
                    compCamera->SetParallelScale(compCamera->GetParallelScale() / delta);
                    impl_->renderComparison();
                }
            }

            emit zoomLevelChanged(impl_->zoomLevel);
        }
    }

    event->accept();
}

// Free functions

std::vector<DRPreset> getStandardDRPresets()
{
    return STANDARD_PRESETS;
}

bool isDRorCRModality(const QString& modality)
{
    return modality == "CR" || modality == "DX" || modality == "DR" ||
           modality == "RG" || modality == "RF";
}

} // namespace dicom_viewer::ui
