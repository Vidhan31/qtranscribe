#include "WhisperModelStorage.h"

#include "LoggingCategories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpHeaders>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

#include <cstring>

using namespace Qt::StringLiterals;

namespace {
const auto kHfBaseUrl = u"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"_s;
} // namespace

WhisperModelStorage::WhisperModelStorage(QObject* parent)
    : QObject(parent) {
    QDir().mkpath(modelsDirectory());
    cleanupOrphanedPartFiles();
    initPresets();
    scanInstalledModels();
    checkDiskSpace();
}

WhisperModelStorage::~WhisperModelStorage() {
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
    const int idx = findModelIndex(modelId);
    if (idx >= 0) {
        return m_models[idx];
    }
    return std::nullopt;
}

int WhisperModelStorage::modelCount() const {
    return static_cast<int>(m_models.size());
}

int WhisperModelStorage::findModelIndex(const QString& modelId) const {
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models[i].id == modelId) {
            return i;
        }
    }
    return -1;
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
    return !m_downloadingModelId.isEmpty();
}

QString WhisperModelStorage::downloadingModelId() const {
    return m_downloadingModelId;
}

qreal WhisperModelStorage::downloadProgress() const {
    return m_currentProgress;
}

QString WhisperModelStorage::downloadSpeedFormatted() const {
    return m_currentSpeedFormatted;
}

QString WhisperModelStorage::downloadBytesFormatted() const {
    if (m_currentTotalBytes > 0) {
        return tr("%1 / %2").arg(formatBytes(m_currentBytesReceived), formatBytes(m_currentTotalBytes));
    }
    if (m_currentBytesReceived > 0) {
        return formatBytes(m_currentBytesReceived);
    }
    return {};
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

    // Pre-flight disk space check (model size + 50 MiB margin)
    const qint64 requiredSpace = model.sizeBytes > 0 ? (model.sizeBytes + 50 * 1024 * 1024) : (100 * 1024 * 1024);
    if (m_availableDiskSpace > 0 && m_availableDiskSpace < requiredSpace) {
        setLastError(tr("Insufficient disk space. %1 required, but only %2 available.")
                         .arg(formatBytes(requiredSpace), formatBytes(m_availableDiskSpace)));
        return false;
    }

    setLastError({});
    m_downloadingModelId = modelId;
    m_downloadAbortReason.clear();
    m_currentProgress = 0.0;
    m_currentBytesReceived = 0;
    m_currentTotalBytes = model.sizeBytes;
    m_currentSpeedFormatted.clear();
    m_lastSpeedBytes = 0;
    m_lastSpeedTimeMs = 0;
    m_downloadTimer.restart();

    const QString partFilePath = modelsDirectory() + u"/"_s + model.fileName + u".part"_s;
    m_partFile = std::make_unique<QFile>(partFilePath);
    if (!m_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(tr("Failed to create temporary file for download: %1").arg(m_partFile->errorString()));
        m_partFile.reset();
        m_downloadingModelId.clear();
        emit isDownloadingAnyChanged();
        return false;
    }

    m_models[idx].isDownloading = true;
    m_models[idx].progress = 0.0;
    m_models[idx].bytesReceived = 0;
    m_models[idx].totalBytes = model.sizeBytes;
    m_models[idx].speedFormatted.clear();

    emit isDownloadingAnyChanged();
    emit downloadProgressChanged(idx, modelId, 0.0, 0, model.sizeBytes, QString());

    QNetworkRequest request(QUrl(model.downloadUrl));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::UserAgent, u"QTranscribe/1.0 (Linux; Wayland)"_s);
    request.setHeaders(headers);

    qCDebug(lcSpeech) << "WhisperModelStorage: Starting download for" << model.id << "from" << model.downloadUrl;

    m_currentReply = m_nam.get(request);
    connect(m_currentReply, &QNetworkReply::readyRead, this, &WhisperModelStorage::onDownloadReadyRead);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &WhisperModelStorage::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &WhisperModelStorage::onDownloadFinished);

    return true;
}

void WhisperModelStorage::cancelDownload(const QString& modelId) {
    if (!isDownloadingAny()) {
        return;
    }

    if (!modelId.isEmpty() && modelId != m_downloadingModelId) {
        return;
    }

    const QString cancelledId = m_downloadingModelId;
    qCDebug(lcSpeech) << "WhisperModelStorage: Cancelling download for" << cancelledId;

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

        emit downloadProgressChanged(idx, cancelledId, 0.0, 0, 0, QString());
    }

    m_downloadingModelId.clear();
    m_downloadAbortReason.clear();
    m_currentProgress = 0.0;
    m_currentBytesReceived = 0;
    m_currentTotalBytes = 0;
    m_currentSpeedFormatted.clear();

    emit isDownloadingAnyChanged();
    checkDiskSpace();
}

bool WhisperModelStorage::deleteModel(const QString& modelId) {
    if (m_downloadingModelId == modelId) {
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

void WhisperModelStorage::onDownloadReadyRead() {
    if (!m_currentReply || !m_partFile || !m_partFile->isOpen() || m_downloadingModelId.isEmpty()) {
        return;
    }

    const int idx = findModelIndex(m_downloadingModelId);
    if (idx < 0) {
        return;
    }

    const auto& model = m_models[idx];
    const QByteArray chunk = m_currentReply->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    const qint64 currentSize = m_partFile->size();
    if (model.sizeBytes > 0 && (currentSize + chunk.size() > model.sizeBytes)) {
        m_downloadAbortReason =
            tr("Download for %1 exceeded catalogued size of %2.").arg(model.name, formatBytes(model.sizeBytes));
        qCWarning(lcSpeech) << "WhisperModelStorage: Download for" << model.id << "exceeded catalogued size ("
                            << (currentSize + chunk.size()) << ">" << model.sizeBytes << "bytes). Aborting.";
        setLastError(m_downloadAbortReason);
        m_currentReply->abort();
        return;
    }

    const qint64 bytesWritten = m_partFile->write(chunk);
    if (bytesWritten != chunk.size() || m_partFile->error() != QFile::NoError) {
        const QString errStr = m_partFile->errorString();
        m_downloadAbortReason = tr("Failed to write download data to disk for %1: %2")
                                    .arg(model.name, errStr.isEmpty() ? tr("Short write or disk error") : errStr);
        qCWarning(lcSpeech) << "WhisperModelStorage: Short write or disk error while downloading" << model.id << ":"
                            << m_downloadAbortReason;
        setLastError(m_downloadAbortReason);
        m_currentReply->abort();
        return;
    }
}

void WhisperModelStorage::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (!m_currentReply || m_downloadingModelId.isEmpty()) {
        return;
    }

    const int idx = findModelIndex(m_downloadingModelId);
    if (idx < 0) {
        return;
    }

    const auto& model = m_models[idx];

    // Enforce catalogued size bound against advertised Content-Length and received bytes
    if (model.sizeBytes > 0 && (bytesTotal > model.sizeBytes || bytesReceived > model.sizeBytes)) {
        m_downloadAbortReason =
            tr("Download size for %1 exceeds catalogued size of %2.").arg(model.name, formatBytes(model.sizeBytes));
        qCWarning(lcSpeech) << "WhisperModelStorage: Reported download size (" << qMax(bytesTotal, bytesReceived)
                            << "bytes) exceeds catalogued size (" << model.sizeBytes << "bytes) for" << model.id
                            << ". Aborting.";
        setLastError(m_downloadAbortReason);
        m_currentReply->abort();
        return;
    }

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

    m_models[idx].progress = m_currentProgress;
    m_models[idx].bytesReceived = bytesReceived;
    m_models[idx].totalBytes = m_currentTotalBytes;
    m_models[idx].speedFormatted = m_currentSpeedFormatted;

    emit downloadProgressChanged(idx, m_downloadingModelId, m_currentProgress, bytesReceived, m_currentTotalBytes,
                                 m_currentSpeedFormatted);
}

void WhisperModelStorage::onDownloadFinished() {
    if (!m_currentReply) {
        return;
    }

    const QString modelId = m_downloadingModelId;
    const int idx = findModelIndex(modelId);
    const QNetworkReply::NetworkError error = m_currentReply->error();
    const int httpStatus = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorString = m_currentReply->errorString();
    const QString abortReason = m_downloadAbortReason;
    m_downloadAbortReason.clear();

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

    bool success = false;
    QString failureMessage;

    if (!abortReason.isEmpty()) {
        failureMessage = abortReason;
    } else if (error != QNetworkReply::NoError || httpStatus < 200 || httpStatus >= 300) {
        const QString fullErr = errorString.isEmpty() ? tr("HTTP error %1").arg(httpStatus) : errorString;
        failureMessage = tr("Download failed for %1: %2").arg(model.name, fullErr);
    } else {
        const QFileInfo partInfo(partPath);
        if (!partInfo.exists()) {
            failureMessage = tr("Downloaded temporary file not found: %1").arg(partPath);
        } else {
            const qint64 actualSize = partInfo.size();
            if (actualSize == 0) {
                failureMessage = tr("Download failed for %1: received empty file.").arg(model.name);
            } else if (model.sizeBytes > 0 && actualSize != model.sizeBytes) {
                failureMessage =
                    tr("Download size mismatch for %1: expected %2 (%3 bytes), but received %4 (%5 bytes).")
                        .arg(model.name, formatBytes(model.sizeBytes), QString::number(model.sizeBytes),
                             formatBytes(actualSize), QString::number(actualSize));
            } else {
                // Validate GGML magic header (0x67676d6c == "ggml")
                QFile verifyFile(partPath);
                if (!verifyFile.open(QIODevice::ReadOnly)) {
                    failureMessage =
                        tr("Failed to open downloaded file for validation: %1").arg(verifyFile.errorString());
                } else {
                    const QByteArray header = verifyFile.read(4);
                    verifyFile.close();

                    constexpr quint32 kGgmlMagic = 0x67676d6c;
                    quint32 magicVal = 0;
                    if (header.size() == 4) {
                        std::memcpy(&magicVal, header.constData(), 4);
                    }

                    if (magicVal != kGgmlMagic) {
                        failureMessage =
                            tr("Download integrity check failed for %1: invalid GGML model format.").arg(model.name);
                    } else {
                        QFile::remove(finalPath);
                        if (QFile::rename(partPath, finalPath)) {
                            success = true;
                        } else {
                            failureMessage =
                                tr("Failed to rename temporary download to destination: %1").arg(finalPath);
                        }
                    }
                }
            }
        }
    }

    if (success) {
        qCDebug(lcSpeech) << "WhisperModelStorage: Successfully validated, downloaded, and finalized" << finalPath;
        m_models[idx].isDownloading = false;
        m_models[idx].isInstalled = true;
        m_models[idx].progress = 1.0;
        const QFileInfo fi(finalPath);
        m_models[idx].installedSizeBytes = fi.size();
        m_models[idx].installedSizeFormatted = formatBytes(fi.size());

        emit modelStatusChanged(idx, modelId, true, m_models[idx].installedSizeBytes);

        m_downloadingModelId.clear();
        emit isDownloadingAnyChanged();
        emit downloadProgressChanged(idx, modelId, 1.0, m_models[idx].installedSizeBytes,
                                     m_models[idx].installedSizeBytes, QString());
        emit downloadFinished(modelId, true, QString());
        checkDiskSpace();
        return;
    }

    qCWarning(lcSpeech) << "WhisperModelStorage: Download failed for" << model.id << ":" << failureMessage;
    setLastError(failureMessage);
    QFile::remove(partPath);

    m_models[idx].isDownloading = false;
    m_models[idx].progress = 0.0;
    m_models[idx].bytesReceived = 0;
    m_models[idx].speedFormatted.clear();

    emit downloadProgressChanged(idx, modelId, 0.0, 0, 0, QString());

    m_downloadingModelId.clear();
    emit isDownloadingAnyChanged();
    emit downloadFinished(modelId, false, failureMessage);
    checkDiskSpace();
}

void WhisperModelStorage::initPresets() {
    m_models = {{u"tiny.en"_s, tr("Tiny (English)"), u"ggml-tiny.en.bin"_s, kHfBaseUrl + u"ggml-tiny.en.bin"_s,
                 77704715, u"~74 MiB"_s, tr("Fastest English dictation with minimal memory (~390 MB RAM)")},
                {u"tiny"_s, tr("Tiny (Multilingual)"), u"ggml-tiny.bin"_s, kHfBaseUrl + u"ggml-tiny.bin"_s, 77691713,
                 u"~74 MiB"_s, tr("Fast multilingual dictation with minimal memory (~390 MB RAM)")},
                {u"base.en"_s, tr("Base (English)"), u"ggml-base.en.bin"_s, kHfBaseUrl + u"ggml-base.en.bin"_s,
                 147964211, u"~141 MiB"_s, tr("Fast English transcription with improved accuracy (~500 MB RAM)")},
                {u"base"_s, tr("Base (Multilingual)"), u"ggml-base.bin"_s, kHfBaseUrl + u"ggml-base.bin"_s, 147951465,
                 u"~141 MiB"_s, tr("Fast multilingual transcription with improved accuracy (~500 MB RAM)")},
                {u"small.en"_s, tr("Small (English)"), u"ggml-small.en.bin"_s, kHfBaseUrl + u"ggml-small.en.bin"_s,
                 487614201, u"~465 MiB"_s, tr("High accuracy English transcription, great balance (~1.0 GB RAM)")},
                {u"small"_s, tr("Small (Multilingual)"), u"ggml-small.bin"_s, kHfBaseUrl + u"ggml-small.bin"_s,
                 487601967, u"~465 MiB"_s, tr("High accuracy multilingual transcription (~1.0 GB RAM)")},
                {u"medium.en"_s, tr("Medium (English)"), u"ggml-medium.en.bin"_s, kHfBaseUrl + u"ggml-medium.en.bin"_s,
                 1533774781, u"~1.4 GiB"_s, tr("Very high accuracy English transcription (~2.6 GB RAM)")},
                {u"medium"_s, tr("Medium (Multilingual)"), u"ggml-medium.bin"_s, kHfBaseUrl + u"ggml-medium.bin"_s,
                 1533763059, u"~1.4 GiB"_s, tr("Very high accuracy multilingual transcription (~2.6 GB RAM)")},
                {u"large-v3-turbo"_s, tr("Large v3 Turbo (Multilingual)"), u"ggml-large-v3-turbo.bin"_s,
                 kHfBaseUrl + u"ggml-large-v3-turbo.bin"_s, 1624555275, u"~1.5 GiB"_s,
                 tr("State-of-the-art accuracy with fast 4-layer decoder (~3.1 GB RAM)")}};
}

void WhisperModelStorage::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged(m_lastError);
    }
}

QString WhisperModelStorage::formatBytes(qint64 bytes) {
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
