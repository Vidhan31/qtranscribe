#include "WhisperModelStorage.h"

#include "LoggingCategories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

WhisperModelStorage::WhisperModelStorage(QObject* parent)
    : WhisperModelStorage(std::make_unique<ModelDownloader>(), parent) { }

WhisperModelStorage::WhisperModelStorage(std::unique_ptr<ModelDownloader> downloader, QObject* parent)
    : QObject(parent)
    , m_downloader(std::move(downloader)) {
    if (!m_downloader) {
        m_downloader = std::make_unique<ModelDownloader>(this);
    } else {
        m_downloader->setParent(this);
    }

    m_models = m_catalog.presets();

    QDir().mkpath(modelsDirectory());
    cleanupOrphanedPartFiles();
    scanInstalledModels();
    checkDiskSpace();

    setupDownloaderConnections();
}

WhisperModelStorage::~WhisperModelStorage() = default;

void WhisperModelStorage::setupDownloaderConnections() {
    connect(m_downloader.get(), &ModelDownloader::isDownloadingAnyChanged, this,
            &WhisperModelStorage::isDownloadingAnyChanged);

    connect(m_downloader.get(), &ModelDownloader::lastErrorChanged, this, &WhisperModelStorage::setLastError);

    connect(m_downloader.get(), &ModelDownloader::downloadProgressChanged, this,
            [this](const QString& modelId, qreal progress, qint64 bytesReceived, qint64 totalBytes,
                   const QString& speedFormatted) {
                const int idx = findModelIndex(modelId);
                if (idx >= 0) {
                    m_models[idx].isDownloading = (progress < 1.0 && bytesReceived > 0);
                    m_models[idx].progress = progress;
                    m_models[idx].bytesReceived = bytesReceived;
                    m_models[idx].totalBytes = totalBytes;
                    m_models[idx].speedFormatted = speedFormatted;
                    emit downloadProgressChanged(idx, modelId, progress, bytesReceived, totalBytes, speedFormatted);
                }
            });

    connect(m_downloader.get(), &ModelDownloader::downloadFinished, this,
            [this](const QString& modelId, bool success, const QString& error) {
                const int idx = findModelIndex(modelId);
                if (idx >= 0) {
                    m_models[idx].isDownloading = false;
                    if (success) {
                        m_models[idx].isInstalled = true;
                        m_models[idx].progress = 1.0;
                        const QFileInfo fi(getModelPath(modelId));
                        m_models[idx].installedSizeBytes = fi.size();
                        m_models[idx].installedSizeFormatted = formatBytes(fi.size());
                        m_models[idx].bytesReceived = fi.size();
                        m_models[idx].totalBytes = fi.size();
                        m_models[idx].speedFormatted.clear();

                        emit modelStatusChanged(idx, modelId, true, m_models[idx].installedSizeBytes);
                        emit downloadProgressChanged(idx, modelId, 1.0, m_models[idx].installedSizeBytes,
                                                     m_models[idx].installedSizeBytes, QString());
                    } else {
                        m_models[idx].progress = 0.0;
                        m_models[idx].bytesReceived = 0;
                        m_models[idx].speedFormatted.clear();
                        emit downloadProgressChanged(idx, modelId, 0.0, 0, 0, QString());
                    }
                }
                emit downloadFinished(modelId, success, error);
                checkDiskSpace();
            });
}

QString WhisperModelStorage::modelsDirectory() const {
    if (!m_customModelsDirectory.isEmpty()) {
        return m_customModelsDirectory;
    }
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return genericData + u"/qtranscribe/models"_s;
}

void WhisperModelStorage::setModelsDirectory(const QString& path) {
    if (m_customModelsDirectory == path) {
        return;
    }
    m_customModelsDirectory = path;
    if (!m_customModelsDirectory.isEmpty()) {
        QDir().mkpath(m_customModelsDirectory);
    }
    cleanupOrphanedPartFiles();
    scanInstalledModels();
    checkDiskSpace();
    emit modelsChanged();
}

const QList<WhisperModelItem>& WhisperModelStorage::models() const {
    return m_models;
}

std::optional<WhisperModelItem> WhisperModelStorage::model(const QString& modelId) const {
    const auto it = std::ranges::find_if(m_models, [&](const auto& m) { return m.id == modelId; });
    return it != m_models.end() ? std::optional(*it) : std::nullopt;
}

int WhisperModelStorage::modelCount() const {
    return static_cast<int>(m_models.size());
}

int WhisperModelStorage::findModelIndex(const QString& modelId) const {
    const auto it = std::ranges::find_if(m_models, [&](const auto& m) { return m.id == modelId; });
    return it != m_models.end() ? static_cast<int>(std::distance(m_models.begin(), it)) : -1;
}

bool WhisperModelStorage::isModelInstalled(const QString& modelId) const {
    const int idx = findModelIndex(modelId);
    if (idx >= 0) {
        return m_models[idx].isInstalled;
    }
    return false;
}

QString WhisperModelStorage::getModelPath(const QString& modelId) const {
    const int idx = findModelIndex(modelId);
    if (idx < 0) {
        return modelsDirectory() + u"/"_s + modelId;
    }

    const QString primary = modelsDirectory() + u"/"_s + m_models[idx].fileName;
    if (QFile::exists(primary)) {
        return primary;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fallback = appData + u"/models/"_s + m_models[idx].fileName;
    if (QFile::exists(fallback)) {
        return fallback;
    }

    return primary;
}

qint64 WhisperModelStorage::availableDiskSpace() const {
    return m_availableDiskSpace;
}

QString WhisperModelStorage::availableDiskSpaceFormatted() const {
    return tr("%1 free").arg(formatBytes(m_availableDiskSpace));
}

bool WhisperModelStorage::isDownloadingAny() const {
    return m_downloader ? m_downloader->isDownloadingAny() : false;
}

QString WhisperModelStorage::downloadingModelId() const {
    return m_downloader ? m_downloader->downloadingModelId() : QString();
}

qreal WhisperModelStorage::downloadProgress() const {
    return m_downloader ? m_downloader->downloadProgress() : 0.0;
}

QString WhisperModelStorage::downloadSpeedFormatted() const {
    return m_downloader ? m_downloader->downloadSpeedFormatted() : QString();
}

QString WhisperModelStorage::downloadBytesFormatted() const {
    return m_downloader ? m_downloader->downloadBytesFormatted() : QString();
}

QString WhisperModelStorage::lastError() const {
    return m_lastError;
}

void WhisperModelStorage::refreshModelList() {
    scanInstalledModels();
    checkDiskSpace();
}

void WhisperModelStorage::scanInstalledModels() {
    const QString primaryDir = modelsDirectory();
    const QString fallbackDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/models"_s;

    for (int i = 0; i < m_models.size(); ++i) {
        auto& item = m_models[i];
        const QString primaryPath = primaryDir + u"/"_s + item.fileName;
        const QString fallbackPath = fallbackDir + u"/"_s + item.fileName;

        QString foundPath;
        if (QFile::exists(primaryPath) && QFileInfo(primaryPath).size() > 0) {
            foundPath = primaryPath;
        } else if (QFile::exists(fallbackPath) && QFileInfo(fallbackPath).size() > 0) {
            foundPath = fallbackPath;
        }

        const bool installed = !foundPath.isEmpty();
        if (item.isInstalled != installed) {
            item.isInstalled = installed;
            if (installed) {
                const QFileInfo fi(foundPath);
                item.installedSizeBytes = fi.size();
                item.installedSizeFormatted = formatBytes(fi.size());
            } else {
                item.installedSizeBytes = 0;
                item.installedSizeFormatted.clear();
            }
            emit modelStatusChanged(i, item.id, item.isInstalled, item.installedSizeBytes);
        }
    }
}

void WhisperModelStorage::checkDiskSpace() {
    const QStorageInfo storage(modelsDirectory());
    const qint64 bytes = storage.bytesAvailable();
    if (m_availableDiskSpace != bytes) {
        m_availableDiskSpace = bytes;
        emit diskSpaceChanged(m_availableDiskSpace);
    }
}

void WhisperModelStorage::cleanupOrphanedPartFiles() {
    QDir dir(modelsDirectory());
    const QStringList partFiles = dir.entryList({u"*.part"_s}, QDir::Files);
    for (const QString& f : partFiles) {
        dir.remove(f);
        qCDebug(lcSpeech) << "WhisperModelStorage: Removed orphaned partial download" << f;
    }
}

bool WhisperModelStorage::startDownload(const QString& modelId) {
    if (isDownloadingAny()) {
        setLastError(tr("Another model download is currently in progress."));
        return false;
    }

    const int idx = findModelIndex(modelId);
    if (idx < 0) {
        setLastError(tr("Model '%1' not found in catalog.").arg(modelId));
        return false;
    }

    checkDiskSpace();
    const auto& model = m_models[idx];

    const qint64 requiredSpace = model.sizeBytes > 0 ? (model.sizeBytes + 50 * 1024 * 1024) : (100 * 1024 * 1024);
    if (m_availableDiskSpace > 0 && m_availableDiskSpace < requiredSpace) {
        setLastError(tr("Insufficient disk space. %1 required, but only %2 available.")
                         .arg(formatBytes(requiredSpace), formatBytes(m_availableDiskSpace)));
        return false;
    }

    setLastError({});

    m_models[idx].isDownloading = true;
    m_models[idx].progress = 0.0;
    m_models[idx].bytesReceived = 0;
    m_models[idx].totalBytes = model.sizeBytes;
    m_models[idx].speedFormatted.clear();

    const QString destinationPath = modelsDirectory() + u"/"_s + model.fileName;
    return m_downloader->startDownload(model.id, model.name, model.downloadUrl, destinationPath, model.sizeBytes);
}

void WhisperModelStorage::cancelDownload(const QString& modelId) {
    if (m_downloader) {
        m_downloader->cancelDownload(modelId);
    }
    checkDiskSpace();
}

bool WhisperModelStorage::deleteModel(const QString& modelId) {
    if (isDownloadingAny() && downloadingModelId() == modelId) {
        cancelDownload(modelId);
    }

    const int idx = findModelIndex(modelId);
    if (idx < 0) {
        return false;
    }

    const auto& model = m_models[idx];
    bool failed = false;
    const QString primaryPath = modelsDirectory() + u"/"_s + model.fileName;
    if (QFile::exists(primaryPath) && !QFile::remove(primaryPath)) {
        failed = true;
    }
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fallbackPath = appData + u"/models/"_s + model.fileName;
    if (QFile::exists(fallbackPath) && !QFile::remove(fallbackPath)) {
        failed = true;
    }

    if (failed) {
        setLastError(tr("Failed to delete model file for '%1'.").arg(model.name));
        scanInstalledModels();
        checkDiskSpace();
        return false;
    }

    m_models[idx].isInstalled = false;
    m_models[idx].installedSizeBytes = 0;
    m_models[idx].installedSizeFormatted.clear();
    emit modelStatusChanged(idx, modelId, false, 0);

    checkDiskSpace();
    return true;
}

void WhisperModelStorage::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged(m_lastError);
    }
}

QString WhisperModelStorage::formatBytes(qint64 bytes) {
    return ModelDownloader::formatBytes(bytes);
}
