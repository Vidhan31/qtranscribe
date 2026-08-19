#pragma once

#include "ApiRequestHandler.h"

#include <QQmlEngine>
#include <QString>

class GroqApiClient;
class QTimer;

class GroqSttClient : public ApiRequestHandler {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(ErrorCategory errorCategory READ errorCategory NOTIFY errorCategoryChanged FINAL)
    Q_PROPERTY(int retrySecondsRemaining READ retrySecondsRemaining NOTIFY retrySecondsRemainingChanged FINAL)
    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged FINAL)
    Q_PROPERTY(QString customPrompt READ customPrompt WRITE setCustomPrompt NOTIFY customPromptChanged FINAL)

public:
    enum class ErrorCategory { None, InvalidApiKey, NetworkOffline, RateLimited, GeneralError };
    Q_ENUM(ErrorCategory)

    explicit GroqSttClient(QObject* parent = nullptr);
    ~GroqSttClient() override = default;

    void setApiClient(GroqApiClient* apiClient);
    GroqApiClient* apiClient() const;

    QString lastError() const;
    ErrorCategory errorCategory() const;
    int retrySecondsRemaining() const;

    QString selectedModel() const;
    void setSelectedModel(const QString& model);

    QString language() const;
    void setLanguage(const QString& lang);

    QString customPrompt() const;
    void setCustomPrompt(const QString& prompt);

    Q_INVOKABLE void transcribe(const QByteArray& wavData, const QString& filename = QStringLiteral("audio.wav"));
    Q_INVOKABLE void retryLast();
    Q_INVOKABLE void cancel() override;

signals:
    void transcriptionReady(const QString& text);
    void errorOccurred(const QString& error);
    void lastErrorChanged();
    void errorCategoryChanged();
    void retrySecondsRemainingChanged();
    void selectedModelChanged();
    void languageChanged();
    void customPromptChanged();

private:
    void setLastError(const QString& error, ErrorCategory category = ErrorCategory::GeneralError);
    void setErrorCategory(ErrorCategory category);
    void setRetrySecondsRemaining(int seconds);
    void sendTranscribeRequest();
    void handleTranscribeResponse(const GroqApiResponse& res);
    ErrorCategory classifyError(const GroqApiResponse& res, QString& outMessage) const;

    GroqApiClient* m_apiClient = nullptr;
    QTimer* m_retryCountdownTimer = nullptr;
    QByteArray m_lastWavData;
    QString m_lastFilename;
    QString m_lastError;
    ErrorCategory m_errorCategory = ErrorCategory::None;
    int m_retrySecondsRemaining = 0;
    QString m_selectedModel;
    QString m_language;
    QString m_customPrompt;

    static constexpr const char* kDefaultModel = "whisper-large-v3-turbo";
};
