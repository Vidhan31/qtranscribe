#pragma once

#include "AbstractCloudSttClient.h"

#include <QQmlEngine>
#include <QString>

class GeminiApiClient;

class GeminiSttClient : public AbstractCloudSttClient {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged FINAL)
    Q_PROPERTY(QString customVocabulary READ customVocabulary WRITE setCustomVocabulary NOTIFY customVocabularyChanged FINAL)

public:
    explicit GeminiSttClient(QObject* parent = nullptr);
    ~GeminiSttClient() override = default;

    void setApiClient(GeminiApiClient* apiClient);
    GeminiApiClient* apiClient() const;

    QString mode() const;
    void setMode(const QString& mode);

    QString customVocabulary() const;
    void setCustomVocabulary(const QString& vocab);

    bool handlesSmartFormatting() const override;

signals:
    void modeChanged();
    void customVocabularyChanged();

protected:
    QString providerDisplayName() const override;
    QString defaultModel() const override;
    QString settingsModelKey() const override;
    QNetworkReply* buildAndSendRequest(const QByteArray& wavData) override;
    QString extractTranscribedText(const CloudApiResponse& res) override;

private:
    QString m_mode;
    QString m_customVocabulary;

    inline static constexpr QStringView kDefaultModel = u"gemini-3.5-transcribe";
    inline static constexpr QStringView kDefaultMode = u"smart";
};
