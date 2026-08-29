#include "GroqSttClient.h"

#include "GroqApiClient.h"
#include "LoggingCategories.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QSettings>

using namespace Qt::StringLiterals;

GroqSttClient::GroqSttClient(QObject* parent)
    : AbstractCloudSttClient(parent) {
    QSettings settings;
    m_selectedModel = settings.value(u"Groq/Model"_s, kDefaultModel.toString()).toString();
    if (m_selectedModel.isEmpty() ||
        (m_selectedModel != u"whisper-large-v3-turbo"_s && m_selectedModel != u"whisper-large-v3"_s)) {
        m_selectedModel = kDefaultModel.toString();
    }
    m_language = settings.value(u"Groq/Language"_s, QString()).toString();
    m_customPrompt = settings.value(u"Groq/CustomPrompt"_s, QString()).toString();
}

void GroqSttClient::setApiClient(GroqApiClient* apiClient) {
    AbstractCloudSttClient::setApiClient(apiClient);
}

GroqApiClient* GroqSttClient::apiClient() const {
    return qobject_cast<GroqApiClient*>(m_apiClient);
}

QString GroqSttClient::providerDisplayName() const {
    return u"Groq"_s;
}

QString GroqSttClient::defaultModel() const {
    return kDefaultModel.toString();
}

QString GroqSttClient::settingsModelKey() const {
    return u"Groq/Model"_s;
}

QString GroqSttClient::language() const {
    return m_language;
}

void GroqSttClient::setLanguage(const QString& lang) {
    QString trimmed = lang.trimmed();
    if (m_language != trimmed) {
        m_language = trimmed;
        QSettings settings;
        settings.setValue(u"Groq/Language"_s, m_language);
        emit languageChanged();
    }
}

QString GroqSttClient::customPrompt() const {
    return m_customPrompt;
}

void GroqSttClient::setCustomPrompt(const QString& prompt) {
    if (m_customPrompt != prompt) {
        m_customPrompt = prompt;
        QSettings settings;
        settings.setValue(u"Groq/CustomPrompt"_s, m_customPrompt);
        emit customPromptChanged();
    }
}

void GroqSttClient::setSelectedModel(const QString& model) {
    QString trimmed = model.trimmed();
    if (trimmed.isEmpty() || (trimmed != u"whisper-large-v3-turbo"_s && trimmed != u"whisper-large-v3"_s)) {
        trimmed = kDefaultModel.toString();
    }
    if (m_selectedModel != trimmed) {
        m_selectedModel = trimmed;
        QSettings settings;
        settings.setValue(u"Groq/Model"_s, m_selectedModel);
        emit selectedModelChanged();
    }
}

void GroqSttClient::transcribe(const QByteArray& wavData) {
    transcribe(wavData, u"audio.wav"_s);
}

void GroqSttClient::transcribe(const QByteArray& wavData, const QString& filename) {
    m_lastFilename = filename;
    AbstractCloudSttClient::transcribe(wavData);
}

QNetworkReply* GroqSttClient::buildAndSendRequest(const QByteArray& wavData) {
    QString modelToUse = m_selectedModel.trimmed().isEmpty() ? kDefaultModel.toString() : m_selectedModel.trimmed();
    QString langToUse = m_language.trimmed();
    QString promptToUse = m_customPrompt.trimmed();
    QString filenameToUse = m_lastFilename.isEmpty() ? u"audio.wav"_s : m_lastFilename;

    qCDebug(lcNetwork) << "Preparing Groq STT multipart request -> Model:" << modelToUse
                       << "Language:" << (langToUse.isEmpty() ? u"Auto-detect"_s : langToUse)
                       << "Prompt set:" << (!promptToUse.isEmpty()) << "Filename:" << filenameToUse
                       << "Audio payload size:" << wavData.size() << "bytes"
                       << "(Retry attempt:" << m_requestRunner.retryCount() << ")";

    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"model\""_s);
    modelPart.setBody(modelToUse.toUtf8());
    multiPart->append(modelPart);

    QHttpPart formatPart;
    formatPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"response_format\""_s);
    formatPart.setBody("json");
    multiPart->append(formatPart);

    if (!langToUse.isEmpty()) {
        QHttpPart langPart;
        langPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"language\""_s);
        langPart.setBody(langToUse.toUtf8());
        multiPart->append(langPart);
    }

    if (!promptToUse.isEmpty()) {
        QHttpPart promptPart;
        promptPart.setHeader(QNetworkRequest::ContentDispositionHeader, u"form-data; name=\"prompt\""_s);
        promptPart.setBody(promptToUse.toUtf8());
        multiPart->append(promptPart);
    }

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       u"form-data; name=\"file\"; filename=\"%1\""_s.arg(filenameToUse));
    filePart.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, u"audio/wav"_s);
    filePart.setBody(wavData);
    multiPart->append(filePart);

    qCDebug(lcNetwork) << "Posting STT request via GroqApiClient";

    return m_apiClient->postMultipart(u"audio/transcriptions"_s, multiPart,
                                      [this](const CloudApiResponse& res) { handleApiResponse(res); });
}

QString GroqSttClient::extractTranscribedText(const CloudApiResponse& res) {
    return res.json.value(u"text"_s).toString();
}
