#include "services/export/measurement_serializer.hpp"

#include <QFile>
#include <QTextStream>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace dicom_viewer::services {

using json = nlohmann::json;

namespace {

QString roiTypeToString(RoiType type) {
    switch (type) {
        case RoiType::Ellipse: return "Ellipse";
        case RoiType::Rectangle: return "Rectangle";
        case RoiType::Polygon: return "Polygon";
        case RoiType::Freehand: return "Freehand";
    }
    return "Unknown";
}

RoiType stringToRoiType(const std::string& str) {
    if (str == "Ellipse") return RoiType::Ellipse;
    if (str == "Rectangle") return RoiType::Rectangle;
    if (str == "Polygon") return RoiType::Polygon;
    if (str == "Freehand") return RoiType::Freehand;
    return RoiType::Rectangle;  // Default
}

json point3DToJson(const Point3D& point) {
    return json::array({point[0], point[1], point[2]});
}

Point3D jsonToPoint3D(const json& j) {
    Point3D point = {0.0, 0.0, 0.0};
    if (j.is_array() && j.size() >= 3) {
        point[0] = j[0].get<double>();
        point[1] = j[1].get<double>();
        point[2] = j[2].get<double>();
    }
    return point;
}

json distanceToJson(const DistanceMeasurement& m) {
    return {
        {"id", m.id},
        {"label", m.label},
        {"point1", point3DToJson(m.point1)},
        {"point2", point3DToJson(m.point2)},
        {"distanceMm", m.distanceMm},
        {"sliceIndex", m.sliceIndex},
        {"visible", m.visible}
    };
}

DistanceMeasurement jsonToDistance(const json& j) {
    DistanceMeasurement m;
    m.id = j.value("id", 0);
    m.label = j.value("label", "");
    m.point1 = jsonToPoint3D(j.value("point1", json::array()));
    m.point2 = jsonToPoint3D(j.value("point2", json::array()));
    m.distanceMm = j.value("distanceMm", 0.0);
    m.sliceIndex = j.value("sliceIndex", -1);
    m.visible = j.value("visible", true);
    return m;
}

json angleToJson(const AngleMeasurement& m) {
    return {
        {"id", m.id},
        {"label", m.label},
        {"vertex", point3DToJson(m.vertex)},
        {"point1", point3DToJson(m.point1)},
        {"point2", point3DToJson(m.point2)},
        {"angleDegrees", m.angleDegrees},
        {"sliceIndex", m.sliceIndex},
        {"visible", m.visible},
        {"isCobbAngle", m.isCobbAngle}
    };
}

AngleMeasurement jsonToAngle(const json& j) {
    AngleMeasurement m;
    m.id = j.value("id", 0);
    m.label = j.value("label", "");
    m.vertex = jsonToPoint3D(j.value("vertex", json::array()));
    m.point1 = jsonToPoint3D(j.value("point1", json::array()));
    m.point2 = jsonToPoint3D(j.value("point2", json::array()));
    m.angleDegrees = j.value("angleDegrees", 0.0);
    m.sliceIndex = j.value("sliceIndex", -1);
    m.visible = j.value("visible", true);
    m.isCobbAngle = j.value("isCobbAngle", false);
    return m;
}

json areaToJson(const AreaMeasurement& m) {
    json pointsArray = json::array();
    for (const auto& pt : m.points) {
        pointsArray.push_back(point3DToJson(pt));
    }

    return {
        {"id", m.id},
        {"label", m.label},
        {"type", roiTypeToString(m.type).toStdString()},
        {"points", pointsArray},
        {"areaMm2", m.areaMm2},
        {"areaCm2", m.areaCm2},
        {"perimeterMm", m.perimeterMm},
        {"centroid", point3DToJson(m.centroid)},
        {"sliceIndex", m.sliceIndex},
        {"visible", m.visible},
        {"semiAxisA", m.semiAxisA},
        {"semiAxisB", m.semiAxisB},
        {"width", m.width},
        {"height", m.height}
    };
}

AreaMeasurement jsonToArea(const json& j) {
    AreaMeasurement m;
    m.id = j.value("id", 0);
    m.label = j.value("label", "");
    m.type = stringToRoiType(j.value("type", "Rectangle"));

    if (j.contains("points") && j["points"].is_array()) {
        for (const auto& pt : j["points"]) {
            m.points.push_back(jsonToPoint3D(pt));
        }
    }

    m.areaMm2 = j.value("areaMm2", 0.0);
    m.areaCm2 = j.value("areaCm2", 0.0);
    m.perimeterMm = j.value("perimeterMm", 0.0);
    m.centroid = jsonToPoint3D(j.value("centroid", json::array()));
    m.sliceIndex = j.value("sliceIndex", -1);
    m.visible = j.value("visible", true);
    m.semiAxisA = j.value("semiAxisA", 0.0);
    m.semiAxisB = j.value("semiAxisB", 0.0);
    m.width = j.value("width", 0.0);
    m.height = j.value("height", 0.0);
    return m;
}

json labelColorToJson(const LabelColor& color) {
    auto rgba = color.toRGBA8();
    return json::array({rgba[0], rgba[1], rgba[2], rgba[3]});
}

LabelColor jsonToLabelColor(const json& j) {
    if (j.is_array() && j.size() >= 4) {
        return LabelColor::fromRGBA8(
            j[0].get<uint8_t>(),
            j[1].get<uint8_t>(),
            j[2].get<uint8_t>(),
            j[3].get<uint8_t>()
        );
    }
    return LabelColor();
}

json labelToJson(const SegmentationLabel& label) {
    return {
        {"id", label.id},
        {"name", label.name},
        {"color", labelColorToJson(label.color)},
        {"opacity", label.opacity},
        {"visible", label.visible}
    };
}

SegmentationLabel jsonToLabel(const json& j) {
    SegmentationLabel label;
    label.id = j.value("id", 0);
    label.name = j.value("name", "");
    label.color = jsonToLabelColor(j.value("color", json::array()));
    label.opacity = j.value("opacity", 1.0);
    label.visible = j.value("visible", true);
    return label;
}

json patientToJson(const PatientInfo& patient) {
    return {
        {"name", patient.name},
        {"patientId", patient.patientId},
        {"dateOfBirth", patient.dateOfBirth},
        {"sex", patient.sex},
        {"studyDate", patient.studyDate},
        {"studyDescription", patient.studyDescription},
        {"modality", patient.modality},
        {"accessionNumber", patient.accessionNumber},
        {"referringPhysician", patient.referringPhysician}
    };
}

PatientInfo jsonToPatient(const json& j) {
    PatientInfo patient;
    patient.name = j.value("name", "");
    patient.patientId = j.value("patientId", "");
    patient.dateOfBirth = j.value("dateOfBirth", "");
    patient.sex = j.value("sex", "");
    patient.studyDate = j.value("studyDate", "");
    patient.studyDescription = j.value("studyDescription", "");
    patient.modality = j.value("modality", "");
    patient.accessionNumber = j.value("accessionNumber", "");
    patient.referringPhysician = j.value("referringPhysician", "");
    return patient;
}

}  // namespace

// =============================================================================
// MeasurementSerializer::Impl
// =============================================================================

class MeasurementSerializer::Impl {
public:
    std::expected<json, SerializationError> sessionToJson(const SessionData& session) const {
        json root;

        // Metadata
        root["version"] = CURRENT_VERSION;
        root["application"] = APPLICATION_ID;
        root["created"] = session.created.isValid()
            ? session.created.toString(Qt::ISODate).toStdString()
            : QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
        root["modified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();

        // Study info
        root["study"] = {
            {"studyInstanceUID", session.studyInstanceUID.toStdString()},
            {"seriesInstanceUID", session.seriesInstanceUID.toStdString()},
            {"patient", patientToJson(session.patient)}
        };

        // Measurements
        json measurements;

        json distances = json::array();
        for (const auto& d : session.distances) {
            distances.push_back(distanceToJson(d));
        }
        measurements["distances"] = distances;

        json angles = json::array();
        for (const auto& a : session.angles) {
            angles.push_back(angleToJson(a));
        }
        measurements["angles"] = angles;

        json areas = json::array();
        for (const auto& a : session.areas) {
            areas.push_back(areaToJson(a));
        }
        measurements["areas"] = areas;

        root["measurements"] = measurements;

        // Segmentation
        json segmentation;
        if (session.labelMapPath.has_value()) {
            segmentation["labelMapPath"] = session.labelMapPath->string();
        }

        json labels = json::array();
        for (const auto& label : session.labels) {
            labels.push_back(labelToJson(label));
        }
        segmentation["labels"] = labels;
        root["segmentation"] = segmentation;

        // View state
        root["viewState"] = {
            {"windowWidth", session.windowWidth},
            {"windowCenter", session.windowCenter},
            {"slicePositions", json::array({
                session.slicePositions[0],
                session.slicePositions[1],
                session.slicePositions[2]
            })}
        };

        return root;
    }

    std::expected<SessionData, SerializationError> jsonToSession(const json& root) const {
        SessionData session;

        // Metadata
        session.version = QString::fromStdString(root.value("version", ""));

        std::string createdStr = root.value("created", "");
        if (!createdStr.empty()) {
            session.created = QDateTime::fromString(
                QString::fromStdString(createdStr), Qt::ISODate);
        }

        std::string modifiedStr = root.value("modified", "");
        if (!modifiedStr.empty()) {
            session.modified = QDateTime::fromString(
                QString::fromStdString(modifiedStr), Qt::ISODate);
        }

        // Study info
        if (root.contains("study")) {
            const auto& study = root["study"];
            session.studyInstanceUID = QString::fromStdString(
                study.value("studyInstanceUID", ""));
            session.seriesInstanceUID = QString::fromStdString(
                study.value("seriesInstanceUID", ""));
            if (study.contains("patient")) {
                session.patient = jsonToPatient(study["patient"]);
            }
        }

        // Measurements
        if (root.contains("measurements")) {
            const auto& measurements = root["measurements"];

            if (measurements.contains("distances") && measurements["distances"].is_array()) {
                for (const auto& d : measurements["distances"]) {
                    session.distances.push_back(jsonToDistance(d));
                }
            }

            if (measurements.contains("angles") && measurements["angles"].is_array()) {
                for (const auto& a : measurements["angles"]) {
                    session.angles.push_back(jsonToAngle(a));
                }
            }

            if (measurements.contains("areas") && measurements["areas"].is_array()) {
                for (const auto& a : measurements["areas"]) {
                    session.areas.push_back(jsonToArea(a));
                }
            }
        }

        // Segmentation
        if (root.contains("segmentation")) {
            const auto& segmentation = root["segmentation"];

            if (segmentation.contains("labelMapPath")) {
                std::string path = segmentation["labelMapPath"].get<std::string>();
                if (!path.empty()) {
                    session.labelMapPath = std::filesystem::path(path);
                }
            }

            if (segmentation.contains("labels") && segmentation["labels"].is_array()) {
                for (const auto& label : segmentation["labels"]) {
                    session.labels.push_back(jsonToLabel(label));
                }
            }
        }

        // View state
        if (root.contains("viewState")) {
            const auto& viewState = root["viewState"];
            session.windowWidth = viewState.value("windowWidth", 400.0);
            session.windowCenter = viewState.value("windowCenter", 40.0);

            if (viewState.contains("slicePositions") &&
                viewState["slicePositions"].is_array() &&
                viewState["slicePositions"].size() >= 3) {
                session.slicePositions[0] = viewState["slicePositions"][0].get<int>();
                session.slicePositions[1] = viewState["slicePositions"][1].get<int>();
                session.slicePositions[2] = viewState["slicePositions"][2].get<int>();
            }
        }

        return session;
    }

    std::expected<bool, SerializationError> validateSchema(const json& root) const {
        // Check required fields
        if (!root.contains("version")) {
            return std::unexpected(SerializationError{
                SerializationError::Code::InvalidSchema,
                "Missing 'version' field"
            });
        }

        std::string version = root["version"].get<std::string>();
        auto supportedVersions = MeasurementSerializer::getSupportedVersions();
        bool versionSupported = false;
        for (const auto& v : supportedVersions) {
            if (v.toStdString() == version) {
                versionSupported = true;
                break;
            }
        }

        if (!versionSupported) {
            return std::unexpected(SerializationError{
                SerializationError::Code::VersionMismatch,
                "Unsupported version: " + version
            });
        }

        if (!root.contains("measurements")) {
            return std::unexpected(SerializationError{
                SerializationError::Code::InvalidSchema,
                "Missing 'measurements' field"
            });
        }

        return true;
    }
};

// =============================================================================
// MeasurementSerializer public methods
// =============================================================================

MeasurementSerializer::MeasurementSerializer() : impl_(std::make_unique<Impl>()) {}

MeasurementSerializer::~MeasurementSerializer() = default;

MeasurementSerializer::MeasurementSerializer(MeasurementSerializer&&) noexcept = default;
MeasurementSerializer& MeasurementSerializer::operator=(MeasurementSerializer&&) noexcept = default;

std::expected<void, SerializationError> MeasurementSerializer::save(
    const SessionData& session,
    const std::filesystem::path& filePath) const {

    // Convert session to JSON
    auto jsonResult = impl_->sessionToJson(session);
    if (!jsonResult) {
        return std::unexpected(jsonResult.error());
    }

    // Write to file
    QFile file(QString::fromStdString(filePath.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return std::unexpected(SerializationError{
            SerializationError::Code::FileAccessDenied,
            "Cannot open file for writing: " + filePath.string()
        });
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    std::string jsonStr = jsonResult->dump(2);  // Pretty print with 2-space indent
    stream << QString::fromStdString(jsonStr);

    file.close();
    return {};
}

std::expected<SessionData, SerializationError> MeasurementSerializer::load(
    const std::filesystem::path& filePath) const {

    // Check file exists
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(SerializationError{
            SerializationError::Code::FileNotFound,
            "File not found: " + filePath.string()
        });
    }

    // Read file
    QFile file(QString::fromStdString(filePath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::unexpected(SerializationError{
            SerializationError::Code::FileAccessDenied,
            "Cannot open file for reading: " + filePath.string()
        });
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    // Parse JSON
    json root;
    try {
        root = json::parse(content.toStdString());
    } catch (const json::parse_error& e) {
        return std::unexpected(SerializationError{
            SerializationError::Code::InvalidJson,
            std::string("JSON parse error: ") + e.what()
        });
    }

    // Validate schema
    auto validationResult = impl_->validateSchema(root);
    if (!validationResult) {
        return std::unexpected(validationResult.error());
    }

    // Convert to session
    return impl_->jsonToSession(root);
}

std::expected<bool, SerializationError> MeasurementSerializer::validate(
    const std::filesystem::path& filePath) const {

    // Check file exists
    if (!std::filesystem::exists(filePath)) {
        return std::unexpected(SerializationError{
            SerializationError::Code::FileNotFound,
            "File not found: " + filePath.string()
        });
    }

    // Read file
    QFile file(QString::fromStdString(filePath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::unexpected(SerializationError{
            SerializationError::Code::FileAccessDenied,
            "Cannot open file for reading: " + filePath.string()
        });
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    file.close();

    // Parse JSON
    json root;
    try {
        root = json::parse(content.toStdString());
    } catch (const json::parse_error& e) {
        return std::unexpected(SerializationError{
            SerializationError::Code::InvalidJson,
            std::string("JSON parse error: ") + e.what()
        });
    }

    // Validate schema
    return impl_->validateSchema(root);
}

bool MeasurementSerializer::isCompatible(
    const SessionData& session,
    const QString& currentStudyUID) {
    return session.studyInstanceUID.isEmpty() ||
           currentStudyUID.isEmpty() ||
           session.studyInstanceUID == currentStudyUID;
}

QString MeasurementSerializer::getFileFilter() {
    return QString("DICOM Viewer Measurements (*%1)").arg(FILE_EXTENSION);
}

std::vector<QString> MeasurementSerializer::getSupportedVersions() {
    return {"1.0.0"};
}

}  // namespace dicom_viewer::services
