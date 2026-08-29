#pragma once

#include "AbstractCloudSttClient.h"
#include "AbstractSttClient.h"

#include <QHash>
#include <QQmlEngine>
#include <QString>

class CloudProviderModel;

class CloudSttRouter : public AbstractSttClient {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool apiKeySet READ isApiKeySet NOTIFY apiKeySetChanged FINAL)
    Q_PROPERTY(bool isApiKeyInvalid READ isApiKeyInvalid NOTIFY isApiKeyInvalidChanged FINAL)
    Q_PROPERTY(bool isRateLimited READ isRateLimited NOTIFY isRateLimitedChanged FINAL)
    Q_PROPERTY(int retrySeconds READ retrySecondsRemaining NOTIFY retrySecondsRemainingChanged FINAL)
    Q_PROPERTY(int retrySecondsRemaining READ retrySecondsRemaining NOTIFY retrySecondsRemainingChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(AbstractCloudSttClient::ErrorCategory errorCategory READ errorCategory NOTIFY errorCategoryChanged FINAL)

public:
    explicit CloudSttRouter(QObject* parent = nullptr);
    ~CloudSttRouter() override = default;

    void setCloudProviderModel(CloudProviderModel* model);
    CloudProviderModel* cloudProviderModel() const;

    void registerProvider(const QString& providerId, AbstractCloudSttClient* client);
    void unregisterProvider(const QString& providerId);

    AbstractCloudSttClient* activeCloudClient() const;
    AbstractSttClient* activeClient() const;

    bool isApiKeySet() const;
    bool isApiKeyInvalid() const;
    bool isRateLimited() const;
    int retrySecondsRemaining() const;
    AbstractCloudSttClient::ErrorCategory errorCategory() const;

    void transcribe(const QByteArray& wavData) override;
    void cancel() override;
    void retryLast() override;
    bool isReady() const override;
    bool isBusy() const override;
    bool handlesSmartFormatting() const override;
    QString lastError() const override;

    void activate() override;
    void deactivate() override;

signals:
    void apiKeySetChanged();
    void isApiKeyInvalidChanged();
    void isRateLimitedChanged();
    void retrySecondsRemainingChanged();
    void lastErrorChanged();
    void errorCategoryChanged();

private slots:
    void onActiveProviderChanged();

private:
    void updateActiveClient();

    CloudProviderModel* m_providerModel = nullptr;
    QHash<QString, AbstractCloudSttClient*> m_clients;
    AbstractCloudSttClient* m_activeClient = nullptr;
};
