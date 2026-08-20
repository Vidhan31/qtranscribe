#include "WhisperSttClient.h"

#include "LoggingCategories.h"

#include "WhisperModelManager.h"
#include "WhisperWorker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
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

void WhisperSttClient::setModelManager(WhisperModelManager* manager) {
    if (m_modelManager == manager) {
        return;
    }

    if (m_modelManager) {
        disconnect(m_modelManager, &WhisperModelManager::selectedModelChanged, this,
                   &WhisperSttClient::checkModelStatus);
        disconnect(m_modelManager, &WhisperModelManager::modelStatusChanged, this, &WhisperSttClient::checkModelStatus);
    }

    m_modelManager = manager;

    if (m_modelManager) {
        connect(m_modelManager, &WhisperModelManager::selectedModelChanged, this, &WhisperSttClient::checkModelStatus);
        connect(m_modelManager, &WhisperModelManager::modelStatusChanged, this, &WhisperSttClient::checkModelStatus);
    }

    checkModelStatus();
}

WhisperModelManager* WhisperSttClient::modelManager() const {
    return m_modelManager;
}

QString WhisperSttClient::modelsDirectory() const {
    if (m_modelManager) {
        return m_modelManager->modelsDirectory();
    }
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return genericData + u"/qtranscribe/models"_s;
}

QString WhisperSttClient::modelFileName() const {
    const QString path = resolveModelPath();
    return QFileInfo(path).fileName();
}

QString WhisperSttClient::resolveModelPath() const {
    if (m_modelManager) {
        return m_modelManager->selectedModelPath();
    }

    const QString primary = modelsDirectory() + u"/ggml-tiny.en.bin"_s;
    if (QFile::exists(primary)) {
        return primary;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fallback = appData + u"/models/ggml-tiny.en.bin"_s;
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

QString WhisperSttClient::loadedModelPath() const {
    return m_loadedModelPath;
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

void WhisperSttClient::activate() {
    if (isModelInstalled() && !m_modelLoaded) {
        loadModel();
    }
    emit readyChanged();
    emit noticeChanged();
}

void WhisperSttClient::deactivate() {
    unloadModel();
    emit readyChanged();
    emit noticeChanged();
}

bool WhisperSttClient::hasNotice() const {
    return !isModelInstalled() || !m_modelLoaded;
}

QVariantMap WhisperSttClient::notice() const {
    QVariantMap noticeMap;
    if (!isModelInstalled()) {
        noticeMap[u"hasNotice"_s] = true;
        noticeMap[u"type"_s] = u"warning"_s;
        noticeMap[u"title"_s] = tr("Offline Whisper Model Missing");
        noticeMap[u"message"_s] = tr("Download %1 to start offline transcription.").arg(modelFileName());
        noticeMap[u"actionText"_s] = tr("Offline Settings");
        noticeMap[u"actionId"_s] = u"openOfflineSettings"_s;
        return noticeMap;
    }

    if (!m_modelLoaded) {
        noticeMap[u"hasNotice"_s] = true;
        noticeMap[u"type"_s] = u"info"_s;
        noticeMap[u"title"_s] = tr("Loading Whisper Model");
        noticeMap[u"message"_s] = tr("Loading offline speech recognition model into memory…");
        noticeMap[u"actionText"_s] = tr("Offline Settings");
        noticeMap[u"actionId"_s] = u"openOfflineSettings"_s;
        return noticeMap;
    }

    return noticeMap;
}

void WhisperSttClient::checkModelStatus() {
    emit modelStatusChanged();
    emit readyChanged();
    emit noticeChanged();
}

void WhisperSttClient::loadModel(const QString& customPath) {
    const QString path = customPath.isEmpty() ? resolveModelPath() : customPath;
    if (!QFile::exists(path)) {
        setLastError(tr("Whisper model file not found at %1. Please download it in Model Settings.").arg(path));
        m_modelLoaded = false;
        m_loadedModelPath.clear();
        emit modelStatusChanged();
        emit readyChanged();
        emit noticeChanged();
        return;
    }

    setLastError({});
    m_loadedModelPath = path;
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

    QSettings settings;
    const QString language = settings.value(u"Groq/Language"_s, QString()).toString();
    const QString customPrompt = settings.value(u"Groq/CustomPrompt"_s, QString()).toString();

    emit requestTranscribe(wavData, language, customPrompt);
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
        m_loadedModelPath.clear();
        setLastError(error.isEmpty() ? tr("Failed to load whisper.cpp model") : error);
    } else {
        setLastError({});
    }

    emit modelStatusChanged();
    emit computeDeviceChanged();
    emit readyChanged();
    emit noticeChanged();
}

void WhisperSttClient::onWorkerModelUnloaded() {
    m_modelLoaded = false;
    m_loadedModelPath.clear();
    m_computeDevice.clear();
    emit modelStatusChanged();
    emit computeDeviceChanged();
    emit readyChanged();
    emit noticeChanged();
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
