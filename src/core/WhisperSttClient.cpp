#include "WhisperSttClient.h"

#include "LoggingCategories.h"

#include "WhisperWorker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

WhisperSttClient::WhisperSttClient(QObject* parent)
    : AbstractSttClient(parent)
    , m_worker(new WhisperWorker()) {
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &WhisperSttClient::requestLoadModel, m_worker, &WhisperWorker::loadModel, Qt::QueuedConnection);
    connect(this, &WhisperSttClient::requestUnloadModel, m_worker, &WhisperWorker::unloadModel, Qt::QueuedConnection);
    connect(this, &WhisperSttClient::requestTranscribe, m_worker, &WhisperWorker::transcribe, Qt::QueuedConnection);
    connect(this, &WhisperSttClient::requestCancel, m_worker, &WhisperWorker::cancel, Qt::QueuedConnection);

    connect(m_worker, &WhisperWorker::modelLoaded, this, &WhisperSttClient::onWorkerModelLoaded, Qt::QueuedConnection);
    connect(m_worker, &WhisperWorker::modelUnloaded, this, &WhisperSttClient::onWorkerModelUnloaded,
            Qt::QueuedConnection);
    connect(m_worker, &WhisperWorker::transcriptionFinished, this, &WhisperSttClient::onWorkerTranscriptionFinished,
            Qt::QueuedConnection);
    connect(m_worker, &WhisperWorker::transcriptionFailed, this, &WhisperSttClient::onWorkerTranscriptionFailed,
            Qt::QueuedConnection);

    m_workerThread.setObjectName(u"WhisperInferenceThread"_s);
    m_workerThread.start();

    // Ensure models directory exists for user convenience
    QDir().mkpath(modelsDirectory());
}

WhisperSttClient::~WhisperSttClient() {
    emit requestCancel();
    emit requestUnloadModel();
    m_workerThread.quit();
    m_workerThread.wait(2000);
}

QString WhisperSttClient::modelsDirectory() const {
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return genericData + u"/qtranscribe/models"_s;
}

QString WhisperSttClient::modelFileName() const {
    return QString::fromLatin1(kModelFileName);
}

QString WhisperSttClient::downloadUrl() const {
    return QString::fromLatin1(kDownloadUrl);
}

QString WhisperSttClient::resolveModelPath() const {
    const QString primary = modelsDirectory() + u"/"_s + QString::fromLatin1(kModelFileName);
    if (QFile::exists(primary)) {
        return primary;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fallback = appData + u"/models/"_s + QString::fromLatin1(kModelFileName);
    if (QFile::exists(fallback)) {
        return fallback;
    }

    return primary;
}

bool WhisperSttClient::isModelInstalled() const {
    return QFile::exists(resolveModelPath());
}

bool WhisperSttClient::isModelLoaded() const {
    return m_modelLoaded;
}

QString WhisperSttClient::modelPath() const {
    return resolveModelPath();
}

QString WhisperSttClient::computeDevice() const {
    return m_computeDevice.isEmpty() ? u"None (Model Unloaded)"_s : m_computeDevice;
}

QString WhisperSttClient::lastError() const {
    return m_lastError;
}

bool WhisperSttClient::isVulkanSupported() const {
#if defined(GGML_USE_VULKAN)
    return true;
#else
    return false;
#endif
}

bool WhisperSttClient::isReady() const {
    return m_modelLoaded && !m_busy;
}

bool WhisperSttClient::isBusy() const {
    return m_busy;
}

void WhisperSttClient::checkModelStatus() {
    emit modelStatusChanged();
    emit readyChanged();
}

void WhisperSttClient::loadModel() {
    const QString path = resolveModelPath();
    if (!QFile::exists(path)) {
        setLastError(
            tr("Whisper model file not found. Please download %1 to %2.").arg(modelFileName(), modelsDirectory()));
        m_modelLoaded = false;
        emit modelStatusChanged();
        emit readyChanged();
        return;
    }

    setLastError({});
    emit requestLoadModel(path, true);
}

void WhisperSttClient::unloadModel() {
    emit requestUnloadModel();
}

void WhisperSttClient::transcribe(const QByteArray& wavData) {
    if (m_busy) {
        qCDebug(lcSpeech) << "WhisperSttClient: transcribe ignored — inference already in progress";
        return;
    }

    if (!isReady()) {
        qWarning() << "WhisperSttClient: transcribe called when not ready (loaded:" << m_modelLoaded << ")";
        const QString err = tr("Offline Whisper model is not loaded or ready");
        setLastError(err);
        emit errorOccurred(err);
        return;
    }

    if (wavData.isEmpty()) {
        qWarning() << "WhisperSttClient: Empty audio payload passed to transcribe";
        const QString err = tr("No audio data to transcribe");
        setLastError(err);
        emit errorOccurred(err);
        return;
    }

    setBusy(true);
    setLastError({});
    emit requestTranscribe(wavData);
}

void WhisperSttClient::cancel() {
    if (m_busy) {
        emit requestCancel();
        setBusy(false);
    }
}

void WhisperSttClient::onWorkerModelLoaded(bool success, const QString& error, const QString& activeDevice) {
    m_modelLoaded = success;
    m_computeDevice = activeDevice;
    if (!success) {
        setLastError(error.isEmpty() ? tr("Failed to load whisper.cpp model") : error);
    } else {
        setLastError({});
    }

    emit modelStatusChanged();
    emit computeDeviceChanged();
    emit readyChanged();
}

void WhisperSttClient::onWorkerModelUnloaded() {
    m_modelLoaded = false;
    m_computeDevice.clear();
    emit modelStatusChanged();
    emit computeDeviceChanged();
    emit readyChanged();
}

void WhisperSttClient::onWorkerTranscriptionFinished(const QString& text) {
    setBusy(false);
    if (text.isEmpty()) {
        const QString err = tr("Whisper returned empty transcription");
        setLastError(err);
        emit errorOccurred(err);
        return;
    }

    setLastError({});
    emit transcriptionReady(text);
}

void WhisperSttClient::onWorkerTranscriptionFailed(const QString& error) {
    setBusy(false);
    setLastError(error);
    emit errorOccurred(error);
}

void WhisperSttClient::setBusy(bool busy) {
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
        emit readyChanged();
    }
}

void WhisperSttClient::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}
