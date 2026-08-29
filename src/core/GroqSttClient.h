#pragma once

#include "AbstractCloudSttClient.h"

#include <QQmlEngine>
#include <QString>

class GroqApiClient;

class GroqSttClient : public AbstractCloudSttClient {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged FINAL)
    Q_PROPERTY(QString customPrompt READ customPrompt WRITE setCustomPrompt NOTIFY customPromptChanged FINAL)

public:
    explicit GroqSttClient(QObject* parent = nullptr);
    ~GroqSttClient() override = default;

    void setApiClient(GroqApiClient* apiClient);
    GroqApiClient* apiClient() const;

    QString language() const;
    void setLanguage(const QString& lang);

    QString customPrompt() const;
    void setCustomPrompt(const QString& prompt);

    void setSelectedModel(const QString& model) override;

    Q_INVOKABLE void transcribe(const QByteArray& wavData) override;
    Q_INVOKABLE void transcribe(const QByteArray& wavData, const QString& filename);

signals:
    void languageChanged();
    void customPromptChanged();

protected:
    QString providerDisplayName() const override;
    QString defaultModel() const override;
    QString settingsModelKey() const override;
    QNetworkReply* buildAndSendRequest(const QByteArray& wavData) override;
    QString extractTranscribedText(const CloudApiResponse& res) override;

private:
    QString m_language;
    QString m_customPrompt;
    QString m_lastFilename;

    inline static constexpr QStringView kDefaultModel = u"whisper-large-v3-turbo";
};
