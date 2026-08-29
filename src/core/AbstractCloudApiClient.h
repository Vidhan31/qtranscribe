#pragma once

#include "CloudResponseParser.h"

#include <QHttpMultiPart>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkRequestFactory>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <chrono>
#include <functional>

class ApiKeyStore;

class AbstractCloudApiClient : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged FINAL)
    Q_PROPERTY(bool apiKeySet READ apiKeySet NOTIFY apiKeySetChanged FINAL)

public:
    using ResponseCallback = std::function<void(const CloudApiResponse&)>;

    explicit AbstractCloudApiClient(const QUrl& baseUrl = QUrl(),
                                    std::chrono::milliseconds timeout = std::chrono::seconds(15),
                                    QObject* parent = nullptr);
    ~AbstractCloudApiClient() override = default;

    void setKeyStore(ApiKeyStore* keyStore);
    ApiKeyStore* keyStore() const;

    QString apiKey() const;
    void setApiKey(const QString& key);
    bool apiKeySet() const;

    Q_INVOKABLE void loadApiKey();
    void ensureApiKeyLoaded();

    void setStorageKeys(const QString& keychainService, const QString& keychainKey,
                        const QString& settingsKey = QString());

    QNetworkAccessManager* networkAccessManager();
    const QNetworkRequestFactory& requestFactory() const;

    QNetworkRequest createApiRequest(const QString& relativePath, const QString& contentType = QString()) const;

    QNetworkReply* postJson(const QString& relativePath, const QJsonObject& body, ResponseCallback callback);
    QNetworkReply* postMultipart(const QString& relativePath, QHttpMultiPart* multiPart, ResponseCallback callback);

    static QString userAgent();

signals:
    void apiKeyChanged();
    void apiKeySetChanged();

protected:
    virtual void updateAuthHeaders();

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkRequestFactory m_requestFactory;
    std::chrono::milliseconds m_timeout = std::chrono::seconds(15);
    ApiKeyStore* m_keyStore = nullptr;
    bool m_ownsKeyStore = false;
};
