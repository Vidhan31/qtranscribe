#include "GeminiSttClient.h"

#include "LoggingCategories.h"

#include "GeminiApiClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>

using namespace Qt::StringLiterals;

GeminiSttClient::GeminiSttClient(QObject* parent)
    : AbstractCloudSttClient(parent) {
    QSettings settings;
    m_selectedModel = settings.value(u"Gemini/Model"_s, kDefaultModel.toString()).toString();
    if (m_selectedModel.trimmed().isEmpty()) {
        m_selectedModel = kDefaultModel.toString();
    }
    m_mode = settings.value(u"Gemini/Mode"_s, kDefaultMode.toString()).toString();
    if (m_mode != u"smart"_s && m_mode != u"verbatim"_s) {
        m_mode = kDefaultMode.toString();
    }
    m_customVocabulary = settings.value(u"Gemini/CustomVocabulary"_s, QString()).toString();
}

void GeminiSttClient::setApiClient(GeminiApiClient* apiClient) {
    AbstractCloudSttClient::setApiClient(apiClient);
}

GeminiApiClient* GeminiSttClient::apiClient() const {
    return qobject_cast<GeminiApiClient*>(m_apiClient);
}

QString GeminiSttClient::providerDisplayName() const {
    return u"Gemini"_s;
}

QString GeminiSttClient::defaultModel() const {
    return kDefaultModel.toString();
}

QString GeminiSttClient::settingsModelKey() const {
    return u"Gemini/Model"_s;
}

QString GeminiSttClient::mode() const {
    return m_mode;
}

void GeminiSttClient::setMode(const QString& mode) {
    QString trimmed = mode.trimmed().toLower();
    if (trimmed != u"smart"_s && trimmed != u"verbatim"_s) {
        trimmed = kDefaultMode.toString();
    }
    if (m_mode != trimmed) {
        m_mode = trimmed;
        QSettings settings;
        settings.setValue(u"Gemini/Mode"_s, m_mode);
        emit modeChanged();
    }
}

QString GeminiSttClient::customVocabulary() const {
    return m_customVocabulary;
}

void GeminiSttClient::setCustomVocabulary(const QString& vocab) {
    if (m_customVocabulary != vocab) {
        m_customVocabulary = vocab;
        QSettings settings;
        settings.setValue(u"Gemini/CustomVocabulary"_s, m_customVocabulary);
        emit customVocabularyChanged();
    }
}

bool GeminiSttClient::handlesSmartFormatting() const {
    return m_mode == u"smart"_s;
}

QNetworkReply* GeminiSttClient::buildAndSendRequest(const QByteArray& wavData) {
    const QString modelToUse =
        m_selectedModel.trimmed().isEmpty() ? kDefaultModel.toString() : m_selectedModel.trimmed();
    const QString modeToUse = (m_mode == u"verbatim"_s) ? u"verbatim"_s : u"smart"_s;

    qCDebug(lcNetwork) << "Preparing Gemini STT request -> Model:" << modelToUse << "Mode:" << modeToUse
                       << "Audio size:" << wavData.size() << "bytes"
                       << "(Retry attempt:" << m_requestRunner.retryCount() << ")";

    QJsonObject rootObj;
    rootObj.insert(u"model"_s, modelToUse);

    QJsonObject audioItem;
    audioItem.insert(u"type"_s, u"audio"_s);
    audioItem.insert(u"data"_s, QString::fromUtf8(wavData.toBase64()));
    audioItem.insert(u"mime_type"_s, u"audio/wav"_s);

    QJsonArray inputArr;
    inputArr.append(audioItem);
    rootObj.insert(u"input"_s, inputArr);

    QJsonObject transcriptionConfig;
    if (modeToUse == u"smart"_s) {
        transcriptionConfig.insert(u"mode"_s, u"smart"_s);
    } else {
        QJsonObject verbatimMode;
        verbatimMode.insert(u"type"_s, u"verbatim"_s);
        transcriptionConfig.insert(u"mode"_s, verbatimMode);
    }

    if (!m_customVocabulary.trimmed().isEmpty()) {
        QJsonArray vocabArr;
        const auto lines = m_customVocabulary.split(QRegularExpression(u"[,;\n\r]+"_s), Qt::SkipEmptyParts);
        for (const auto& token : lines) {
            const QString item = token.trimmed();
            if (!item.isEmpty()) {
                vocabArr.append(item);
            }
        }
        if (!vocabArr.isEmpty()) {
            transcriptionConfig.insert(u"custom_vocabulary"_s, vocabArr);
        }
    }

    QJsonObject generationConfig;
    generationConfig.insert(u"transcription_config"_s, transcriptionConfig);
    rootObj.insert(u"generation_config"_s, generationConfig);

    return m_apiClient->postJson(u"interactions"_s, rootObj,
                                 [this](const CloudApiResponse& res) { handleApiResponse(res); });
}

QString GeminiSttClient::extractTranscribedText(const CloudApiResponse& res) {
    const QJsonObject& json = res.json;
    if (json.contains(u"output_text"_s)) {
        const QString directText = json.value(u"output_text"_s).toString().trimmed();
        if (!directText.isEmpty()) {
            return directText;
        }
    }

    if (json.contains(u"steps"_s)) {
        const QJsonArray steps = json.value(u"steps"_s).toArray();
        QStringList extractedParts;
        for (const auto& stepVal : steps) {
            const QJsonObject stepObj = stepVal.toObject();
            if (stepObj.value(u"type"_s).toString() == u"model_output"_s && stepObj.contains(u"content"_s)) {
                const QJsonArray contentArr = stepObj.value(u"content"_s).toArray();
                for (const auto& contentVal : contentArr) {
                    const QJsonObject contentObj = contentVal.toObject();
                    if (contentObj.value(u"type"_s).toString() == u"text"_s) {
                        extractedParts.append(contentObj.value(u"text"_s).toString());
                    }
                }
            }
        }
        const QString combined = extractedParts.join(u"\n"_s).trimmed();
        if (!combined.isEmpty()) {
            return combined;
        }
    }

    return {};
}
