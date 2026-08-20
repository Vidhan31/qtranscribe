#include "WhisperModelManager.h"

#include "LoggingCategories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace {
constexpr const char* kHfBaseUrl = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/";
constexpr const char* kDefaultSelectedModel = "tiny.en";
} // namespace

WhisperModelManager::WhisperModelManager(QObject* parent)
    : QAbstractListModel(parent) {
    // Ensure models directory exists
    QDir().mkpath(modelsDirectory());

    // Clean up any stale partial downloads left over from previous runs
    cleanupOrphanedPartFiles();

    // Initialize presets
    initPresets();
    scanInstalledModels();
    checkDiskSpace();

    // Restore selected model from settings
    QSettings settings;
    m_selectedModelId =
        settings.value(u"Whisper/SelectedModel"_s, QString::fromLatin1(kDefaultSelectedModel)).toString();
    if (findModelIndex(m_selectedModelId) < 0 && !m_models.isEmpty()) {
        m_selectedModelId = m_models.first().id;
    }
}

WhisperModelManager::~WhisperModelManager() {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
    if (m_partFile) {
        m_partFile->close();
        if (!m_downloadingModelId.isEmpty()) {
            const int idx = findModelIndex(m_downloadingModelId);
            if (idx >= 0) {
                const QString partPath = modelsDirectory() + u"/"_s + m_models[idx].fileName + u".part"_s;
                QFile::remove(partPath);
            }
        }
    }
}

int WhisperModelManager::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_models.size());
}

QVariant WhisperModelManager::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_models.size()) {
        return {};
    }

    const auto& item = m_models.at(index.row());
    switch (role) {
        case IdRole:
            return item.id;
        case NameRole:
            return item.name;
        case FileNameRole:
            return item.fileName;
        case DownloadUrlRole:
            return item.downloadUrl;
        case SizeBytesRole:
            return item.sizeBytes;
        case SizeFormattedRole:
            return item.sizeFormatted;
        case DescriptionRole:
            return item.description;
        case IsInstalledRole:
            return item.isInstalled;
        case IsSelectedRole:
            return item.id == m_selectedModelId;
        case IsDownloadingRole:
            return item.isDownloading;
        case ProgressRole:
            return item.progress;
        case BytesReceivedRole:
            return item.bytesReceived;
        case TotalBytesRole:
            return item.totalBytes;
        case SpeedFormattedRole:
            return item.speedFormatted;
        case InstalledSizeFormattedRole:
            return item.installedSizeFormatted;
        case CanDeleteRole:
            return item.isInstalled;
        default:
            return {};
    }
}

QHash<int, QByteArray> WhisperModelManager::roleNames() const {
    return {{IdRole, "modelId"},
            {NameRole, "name"},
            {FileNameRole, "fileName"},
            {DownloadUrlRole, "downloadUrl"},
            {SizeBytesRole, "sizeBytes"},
            {SizeFormattedRole, "sizeFormatted"},
            {DescriptionRole, "description"},
            {IsInstalledRole, "isInstalled"},
            {IsSelectedRole, "isSelected"},
            {IsDownloadingRole, "isDownloading"},
            {ProgressRole, "progress"},
            {BytesReceivedRole, "bytesReceived"},
            {TotalBytesRole, "totalBytes"},
            {SpeedFormattedRole, "speedFormatted"},
            {InstalledSizeFormattedRole, "installedSizeFormatted"},
            {CanDeleteRole, "canDelete"}};
}

QString WhisperModelManager::modelsDirectory() const {
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return genericData + u"/qtranscribe/models"_s;
}

QString WhisperModelManager::selectedModelId() const {
    return m_selectedModelId;
}

void WhisperModelManager::setSelectedModelId(const QString& id) {
    if (m_selectedModelId != id) {
        const int oldIdx = findModelIndex(m_selectedModelId);
        m_selectedModelId = id;
        const int newIdx = findModelIndex(m_selectedModelId);

        QSettings settings;
        settings.setValue(u"Whisper/SelectedModel"_s, m_selectedModelId);

        if (oldIdx >= 0) {
            const QModelIndex modelIdx = index(oldIdx);
            emit dataChanged(modelIdx, modelIdx, {IsSelectedRole});
        }
        if (newIdx >= 0) {
            const QModelIndex modelIdx = index(newIdx);
            emit dataChanged(modelIdx, modelIdx, {IsSelectedRole});
        }

        emit selectedModelChanged();
        emit modelStatusChanged();
    }
}

QString WhisperModelManager::selectedModelPath() const {
    return getModelPath(m_selectedModelId);
}

QString WhisperModelManager::selectedModelName() const {
    const int idx = findModelIndex(m_selectedModelId);
    if (idx >= 0) {
        return m_models[idx].name;
    }
    return m_selectedModelId;
}

bool WhisperModelManager::isSelectedModelInstalled() const {
    return isModelInstalled(m_selectedModelId);
}

bool WhisperModelManager::isDownloadingAny() const {
    return !m_downloadingModelId.isEmpty();
}

QString WhisperModelManager::downloadingModelId() const {
    return m_downloadingModelId;
}

qreal WhisperModelManager::downloadProgress() const {
    return m_currentProgress;
}

QString WhisperModelManager::downloadSpeedFormatted() const {
    return m_currentSpeedFormatted;
}

QString WhisperModelManager::downloadBytesFormatted() const {
    if (m_currentTotalBytes > 0) {
        return tr("%1 / %2").arg(formatBytes(m_currentBytesReceived), formatBytes(m_currentTotalBytes));
    }
    if (m_currentBytesReceived > 0) {
        return formatBytes(m_currentBytesReceived);
    }
    return {};
}

QString WhisperModelManager::lastError() const {
    return m_lastError;
}

QString WhisperModelManager::availableDiskSpaceFormatted() const {
    return tr("%1 free").arg(formatBytes(m_availableDiskSpace));
}

bool WhisperModelManager::isModelInstalled(const QString& modelId) const {
    const int idx = findModelIndex(modelId);
    if (idx >= 0) {
        return m_models[idx].isInstalled;
    }
    return false;
}

QString WhisperModelManager::getModelPath(const QString& modelId) const {
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

void WhisperModelManager::startDownload(const QString& modelId) {
    if (isDownloadingAny()) {
        setLastError(tr("Another model download is currently in progress."));
        return;
    }

    const int idx = findModelIndex(modelId);
    if (idx < 0) {
        setLastError(tr("Model '%1' not found in catalog.").arg(modelId));
        return;
    }

    checkDiskSpace();
    const auto& model = m_models[idx];

    // Pre-flight disk space check (model size + 50 MiB margin)
    const qint64 requiredSpace = model.sizeBytes > 0 ? (model.sizeBytes + 50 * 1024 * 1024) : (100 * 1024 * 1024);
    if (m_availableDiskSpace > 0 && m_availableDiskSpace < requiredSpace) {
        setLastError(tr("Insufficient disk space. %1 required, but only %2 available.")
                         .arg(formatBytes(requiredSpace), formatBytes(m_availableDiskSpace)));
        return;
    }

    setLastError({});
    m_downloadingModelId = modelId;
    m_currentProgress = 0.0;
    m_currentBytesReceived = 0;
    m_currentTotalBytes = model.sizeBytes;
    m_currentSpeedFormatted.clear();
    m_lastSpeedBytes = 0;
    m_lastSpeedTimeMs = 0;
    m_downloadTimer.restart();

    // Prepare .part file
    const QString partFilePath = modelsDirectory() + u"/"_s + model.fileName + u".part"_s;
    m_partFile.reset(new QFile(partFilePath));
    if (!m_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(tr("Failed to create temporary file for download: %1").arg(m_partFile->errorString()));
        m_partFile.reset();
        m_downloadingModelId.clear();
        emit isDownloadingAnyChanged();
        return;
    }

    m_models[idx].isDownloading = true;
    m_models[idx].progress = 0.0;
    m_models[idx].bytesReceived = 0;
    m_models[idx].totalBytes = model.sizeBytes;
    m_models[idx].speedFormatted.clear();

    const QModelIndex modelIdx = index(idx);
    emit dataChanged(modelIdx, modelIdx,
                     {IsDownloadingRole, ProgressRole, BytesReceivedRole, TotalBytesRole, SpeedFormattedRole});
    emit isDownloadingAnyChanged();
    emit downloadProgressChanged();

    // Build HTTP request
    QNetworkRequest request(QUrl(model.downloadUrl));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"QTranscribe/1.0 (Linux; Wayland)"_s);

    qCDebug(lcSpeech) << "WhisperModelManager: Starting download for" << model.id << "from" << model.downloadUrl;

    m_currentReply = m_nam.get(request);
    connect(m_currentReply, &QNetworkReply::readyRead, this, &WhisperModelManager::onDownloadReadyRead);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &WhisperModelManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &WhisperModelManager::onDownloadFinished);
}

void WhisperModelManager::cancelDownload(const QString& modelId) {
    if (!isDownloadingAny()) {
        return;
    }

    if (!modelId.isEmpty() && modelId != m_downloadingModelId) {
        return;
    }

    const QString cancelledId = m_downloadingModelId;
    qCDebug(lcSpeech) << "WhisperModelManager: Cancelling download for" << cancelledId;

    if (m_currentReply) {
        m_currentReply->disconnect(this);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_partFile) {
        m_partFile->close();
        m_partFile.reset();
    }

    const int idx = findModelIndex(cancelledId);
    if (idx >= 0) {
        const QString partPath = modelsDirectory() + u"/"_s + m_models[idx].fileName + u".part"_s;
        QFile::remove(partPath);

        m_models[idx].isDownloading = false;
        m_models[idx].progress = 0.0;
        m_models[idx].bytesReceived = 0;
        m_models[idx].speedFormatted.clear();

        const QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx, {IsDownloadingRole, ProgressRole, BytesReceivedRole, SpeedFormattedRole});
    }

    m_downloadingModelId.clear();
    m_currentProgress = 0.0;
    m_currentBytesReceived = 0;
    m_currentTotalBytes = 0;
    m_currentSpeedFormatted.clear();

    emit isDownloadingAnyChanged();
    emit downloadProgressChanged();
    checkDiskSpace();
}

bool WhisperModelManager::deleteModel(const QString& modelId) {
    if (m_downloadingModelId == modelId) {
        cancelDownload(modelId);
    }

    const int idx = findModelIndex(modelId);
    if (idx < 0) {
        return false;
    }

    const auto& model = m_models[idx];
    const QString primaryPath = modelsDirectory() + u"/"_s + model.fileName;
    if (QFile::exists(primaryPath)) {
        QFile::remove(primaryPath);
    }

    m_models[idx].isInstalled = false;
    m_models[idx].installedSizeBytes = 0;
    m_models[idx].installedSizeFormatted.clear();
    const QModelIndex modelIdx = index(idx);
    emit dataChanged(modelIdx, modelIdx, {IsInstalledRole, InstalledSizeFormattedRole, CanDeleteRole});

    checkDiskSpace();
    emit modelStatusChanged();
    emit selectedModelChanged();
    return true;
}

void WhisperModelManager::refreshModelList() {
    scanInstalledModels();
    checkDiskSpace();
}

void WhisperModelManager::checkDiskSpace() {
    const QStorageInfo storage(modelsDirectory());
    const qint64 bytes = storage.bytesAvailable();
    if (m_availableDiskSpace != bytes) {
        m_availableDiskSpace = bytes;
        emit diskSpaceChanged();
    }
}

void WhisperModelManager::onDownloadReadyRead() {
    if (!m_currentReply || !m_partFile || !m_partFile->isOpen()) {
        return;
    }

    const QByteArray chunk = m_currentReply->readAll();
    if (!chunk.isEmpty()) {
        m_partFile->write(chunk);
    }
}

void WhisperModelManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    m_currentBytesReceived = bytesReceived;
    if (bytesTotal > 0) {
        m_currentTotalBytes = bytesTotal;
        m_currentProgress = static_cast<qreal>(bytesReceived) / static_cast<qreal>(bytesTotal);
    }

    const qint64 elapsedMs = m_downloadTimer.elapsed();
    if (elapsedMs - m_lastSpeedTimeMs >= 250 && elapsedMs > 0) {
        const qint64 bytesDelta = bytesReceived - m_lastSpeedBytes;
        const qint64 timeDeltaMs = elapsedMs - m_lastSpeedTimeMs;
        if (timeDeltaMs > 0) {
            const qreal speedBytesPerSec = (static_cast<qreal>(bytesDelta) * 1000.0) / static_cast<qreal>(timeDeltaMs);
            m_currentSpeedFormatted = tr("%1/s").arg(formatBytes(static_cast<qint64>(speedBytesPerSec)));
        }
        m_lastSpeedBytes = bytesReceived;
        m_lastSpeedTimeMs = elapsedMs;
    }

    const int idx = findModelIndex(m_downloadingModelId);
    if (idx >= 0) {
        m_models[idx].progress = m_currentProgress;
        m_models[idx].bytesReceived = bytesReceived;
        m_models[idx].totalBytes = m_currentTotalBytes;
        m_models[idx].speedFormatted = m_currentSpeedFormatted;

        const QModelIndex modelIdx = index(idx);
        emit dataChanged(modelIdx, modelIdx, {ProgressRole, BytesReceivedRole, TotalBytesRole, SpeedFormattedRole});
    }

    emit downloadProgressChanged();
}

void WhisperModelManager::onDownloadFinished() {
    if (!m_currentReply) {
        return;
    }

    const QString modelId = m_downloadingModelId;
    const int idx = findModelIndex(modelId);
    const QNetworkReply::NetworkError error = m_currentReply->error();
    const int httpStatus = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorString = m_currentReply->errorString();

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (m_partFile) {
        m_partFile->flush();
        m_partFile->close();
        m_partFile.reset();
    }

    if (idx < 0) {
        m_downloadingModelId.clear();
        emit isDownloadingAnyChanged();
        return;
    }

    const auto& model = m_models[idx];
    const QString partPath = modelsDirectory() + u"/"_s + model.fileName + u".part"_s;
    const QString finalPath = modelsDirectory() + u"/"_s + model.fileName;

    if (error == QNetworkReply::NoError && (httpStatus >= 200 && httpStatus < 300)) {
        QFile::remove(finalPath);
        if (QFile::rename(partPath, finalPath)) {
            qCDebug(lcSpeech) << "WhisperModelManager: Successfully downloaded and finalized" << finalPath;
            m_models[idx].isDownloading = false;
            m_models[idx].isInstalled = true;
            m_models[idx].progress = 1.0;
            const QFileInfo fi(finalPath);
            m_models[idx].installedSizeBytes = fi.size();
            m_models[idx].installedSizeFormatted = formatBytes(fi.size());

            const QModelIndex modelIdx = index(idx);
            emit dataChanged(
                modelIdx, modelIdx,
                {IsDownloadingRole, IsInstalledRole, ProgressRole, InstalledSizeFormattedRole, CanDeleteRole});

            m_downloadingModelId.clear();
            emit isDownloadingAnyChanged();
            emit downloadProgressChanged();
            emit modelStatusChanged();
            emit selectedModelChanged();
            emit modelDownloadFinished(modelId, true, QString());
            checkDiskSpace();
            return;
        } else {
            setLastError(tr("Failed to rename temporary download to destination: %1").arg(finalPath));
        }
    } else {
        const QString fullErr = errorString.isEmpty() ? tr("HTTP error %1").arg(httpStatus) : errorString;
        setLastError(tr("Download failed for %1: %2").arg(model.name, fullErr));
    }

    // Cleanup partial file on error
    QFile::remove(partPath);
    m_models[idx].isDownloading = false;
    m_models[idx].progress = 0.0;
    m_models[idx].bytesReceived = 0;
    m_models[idx].speedFormatted.clear();

    const QModelIndex modelIdx = index(idx);
    emit dataChanged(modelIdx, modelIdx, {IsDownloadingRole, ProgressRole, BytesReceivedRole, SpeedFormattedRole});

    m_downloadingModelId.clear();
    emit isDownloadingAnyChanged();
    emit downloadProgressChanged();
    emit modelDownloadFinished(modelId, false, m_lastError);
    checkDiskSpace();
}

void WhisperModelManager::initPresets() {
    m_models = {
        {u"tiny.en"_s, tr("Tiny (English)"), u"ggml-tiny.en.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-tiny.en.bin"_s, 77651000, u"~75 MiB"_s,
         tr("Fastest English dictation with minimal memory (~390 MB RAM)")},
        {u"tiny"_s, tr("Tiny (Multilingual)"), u"ggml-tiny.bin"_s, QString::fromLatin1(kHfBaseUrl) + u"ggml-tiny.bin"_s,
         77770000, u"~75 MiB"_s, tr("Fast multilingual dictation with minimal memory (~390 MB RAM)")},
        {u"base.en"_s, tr("Base (English)"), u"ggml-base.en.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-base.en.bin"_s, 147951000, u"~142 MiB"_s,
         tr("Fast English transcription with improved accuracy (~500 MB RAM)")},
        {u"base"_s, tr("Base (Multilingual)"), u"ggml-base.bin"_s, QString::fromLatin1(kHfBaseUrl) + u"ggml-base.bin"_s,
         147964000, u"~142 MiB"_s, tr("Fast multilingual transcription with improved accuracy (~500 MB RAM)")},
        {u"small.en"_s, tr("Small (English)"), u"ggml-small.en.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-small.en.bin"_s, 487600000, u"~466 MiB"_s,
         tr("High accuracy English transcription, great balance (~1.0 GB RAM)")},
        {u"small"_s, tr("Small (Multilingual)"), u"ggml-small.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-small.bin"_s, 487600000, u"~466 MiB"_s,
         tr("High accuracy multilingual transcription (~1.0 GB RAM)")},
        {u"medium.en"_s, tr("Medium (English)"), u"ggml-medium.en.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-medium.en.bin"_s, 1533000000, u"~1.5 GiB"_s,
         tr("Very high accuracy English transcription (~2.6 GB RAM)")},
        {u"medium"_s, tr("Medium (Multilingual)"), u"ggml-medium.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-medium.bin"_s, 1533000000, u"~1.5 GiB"_s,
         tr("Very high accuracy multilingual transcription (~2.6 GB RAM)")},
        {u"large-v3-turbo"_s, tr("Large v3 Turbo (Multilingual)"), u"ggml-large-v3-turbo.bin"_s,
         QString::fromLatin1(kHfBaseUrl) + u"ggml-large-v3-turbo.bin"_s, 1620000000, u"~1.6 GiB"_s,
         tr("State-of-the-art accuracy with fast 4-layer decoder (~3.1 GB RAM)")}};
}

void WhisperModelManager::scanInstalledModels() {
    const QString primaryDir = modelsDirectory();
    const QString fallbackDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/models"_s;

    for (int i = 0; i < m_models.size(); ++i) {
        auto& item = m_models[i];
        const QString primaryPath = primaryDir + u"/"_s + item.fileName;
        const QString fallbackPath = fallbackDir + u"/"_s + item.fileName;

        QString foundPath;
        if (QFile::exists(primaryPath)) {
            foundPath = primaryPath;
        } else if (QFile::exists(fallbackPath)) {
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
            const QModelIndex modelIdx = index(i);
            emit dataChanged(modelIdx, modelIdx, {IsInstalledRole, InstalledSizeFormattedRole, CanDeleteRole});
        }
    }

    emit modelStatusChanged();
    emit selectedModelChanged();
}

void WhisperModelManager::cleanupOrphanedPartFiles() {
    QDir dir(modelsDirectory());
    const QStringList partFiles = dir.entryList({u"*.part"_s}, QDir::Files);
    for (const QString& f : partFiles) {
        dir.remove(f);
        qCDebug(lcSpeech) << "WhisperModelManager: Removed orphaned partial download" << f;
    }
}

void WhisperModelManager::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}

int WhisperModelManager::findModelIndex(const QString& modelId) const {
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models[i].id == modelId) {
            return i;
        }
    }
    return -1;
}

QString WhisperModelManager::formatBytes(qint64 bytes) {
    if (bytes < 0) {
        return u"0 B"_s;
    }
    const qreal b = static_cast<qreal>(bytes);
    if (b < 1024.0) {
        return QString::number(bytes) + u" B"_s;
    }
    if (b < 1024.0 * 1024.0) {
        return QString::number(b / 1024.0, 'f', 1) + u" KiB"_s;
    }
    if (b < 1024.0 * 1024.0 * 1024.0) {
        return QString::number(b / (1024.0 * 1024.0), 'f', 1) + u" MiB"_s;
    }
    return QString::number(b / (1024.0 * 1024.0 * 1024.0), 'f', 2) + u" GiB"_s;
}
