#pragma once

#include "AbstractSttClient.h"
#include "CloudResponseParser.h"
#include "HttpRequestRunner.h"

#include <QQmlEngine>
#include <QString>

class AbstractCloudApiClient;
class QTimer;

class AbstractCloudSttClient : public AbstractSttClient {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(bool apiKeySet READ isApiKeySet NOTIFY apiKeySetChanged FINAL)
    Q_PROPERTY(bool isApiKeyInvalid READ isApiKeyInvalid NOTIFY isApiKeyInvalidChanged FINAL)
    Q_PROPERTY(bool isRateLimited READ isRateLimited NOTIFY isRateLimitedChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(ErrorCategory errorCategory READ errorCategory NOTIFY errorCategoryChanged FINAL)
    Q_PROPERTY(int retrySecondsRemaining READ retrySecondsRemaining NOTIFY retrySecondsRemainingChanged FINAL)
    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged FINAL)

public:
    enum class ErrorCategory { None, InvalidApiKey, NetworkOffline, RateLimited, GeneralError };
    Q_ENUM(ErrorCategory)

    explicit AbstractCloudSttClient(QObject* parent = nullptr);
    ~AbstractCloudSttClient() override = default;

    void setApiClient(AbstractCloudApiClient* apiClient);
    AbstractCloudApiClient* apiClient() const;

    bool isApiKeySet() const;
    bool isApiKeyInvalid() const;
    bool isRateLimited() const;

    QString lastError() const override;
    ErrorCategory errorCategory() const;
    int retrySecondsRemaining() const;

    QString selectedModel() const;
    virtual void setSelectedModel(const QString& model);

    bool isReady() const override;
    bool isBusy() const override;
    bool isCancelled() const;

    void activate() override;
    void deactivate() override;

    Q_INVOKABLE void transcribe(const QByteArray& wavData) override;
    Q_INVOKABLE void retryLast() override;
    Q_INVOKABLE void cancel() override;

signals:
    void apiKeySetChanged();
    void isApiKeyInvalidChanged();
    void isRateLimitedChanged();
    void lastErrorChanged();
    void errorCategoryChanged();
    void retrySecondsRemainingChanged();
    void selectedModelChanged();

protected slots:
    void onApiKeySetChanged();

protected:
    void setBusy(bool busy);
    void setLastError(const QString& error, ErrorCategory category = ErrorCategory::GeneralError);
    void setErrorCategory(ErrorCategory category);
    void setRetrySecondsRemaining(int seconds);
    void sendTranscribeRequest();
    void handleApiResponse(const CloudApiResponse& res);
    virtual ErrorCategory classifyError(const CloudApiResponse& res, QString& outMessage) const;

    virtual QString providerDisplayName() const = 0;
    virtual QString defaultModel() const = 0;
    virtual QString settingsModelKey() const = 0;
    virtual QNetworkReply* buildAndSendRequest(const QByteArray& wavData) = 0;
    virtual QString extractTranscribedText(const CloudApiResponse& res) = 0;

    AbstractCloudApiClient* m_apiClient = nullptr;
    QTimer* m_retryCountdownTimer = nullptr;
    QTimer* m_retryTimer = nullptr;
    HttpRequestRunner m_requestRunner;
    QByteArray m_lastWavData;
    QString m_lastError;
    ErrorCategory m_errorCategory = ErrorCategory::None;
    int m_retrySecondsRemaining = 0;
    QString m_selectedModel;
};
