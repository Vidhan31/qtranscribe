#include "AbstractCloudApiClient.h"

#include "LoggingCategories.h"

#include "ApiKeyStore.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHttpHeaders>
#include <QJsonDocument>
#include <QNetworkProxyFactory>
#include <QSslConfiguration>
#include <QSslSocket>

#include <memory>

using namespace Qt::StringLiterals;

AbstractCloudApiClient::AbstractCloudApiClient(const QUrl& baseUrl, std::chrono::milliseconds timeout, QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timeout(timeout) {
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    sslConfig.setAllowedNextProtocols({QSslConfiguration::ALPNProtocolHTTP2});
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    connect(m_nam, &QNetworkAccessManager::sslErrors, this,
            [](QNetworkReply* /*reply*/, const QList<QSslError>& errors) {
                for (const auto& err : errors) {
                    qWarning() << "AbstractCloudApiClient SSL Error:" << err.errorString();
                }
            });

    m_nam->setTransferTimeout(m_timeout);

    if (!baseUrl.isEmpty()) {
        m_requestFactory.setBaseUrl(baseUrl);
    }
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::UserAgent, userAgent());
    m_requestFactory.setCommonHeaders(headers);

    auto* defaultStore = new ApiKeyStore(this);
    setKeyStore(defaultStore);
    m_ownsKeyStore = true;
}

#ifndef QTRANSCRIBE_VERSION
#define QTRANSCRIBE_VERSION "0.0.0-dev"
#endif

QString AbstractCloudApiClient::userAgent() {
    const QString appVer = QCoreApplication::applicationVersion().isEmpty() ? u"" QTRANSCRIBE_VERSION ""_s
                                                                            : QCoreApplication::applicationVersion();
    return u"QTranscribe/%1 (Linux; Qt %2)"_s.arg(appVer, QString::fromUtf8(QT_VERSION_STR));
}

void AbstractCloudApiClient::updateAuthHeaders() { }

void AbstractCloudApiClient::setKeyStore(ApiKeyStore* keyStore) {
    if (m_keyStore == keyStore) {
        return;
    }

    if (m_keyStore) {
        disconnect(m_keyStore, &ApiKeyStore::apiKeyChanged, this, nullptr);
        disconnect(m_keyStore, &ApiKeyStore::apiKeySetChanged, this, nullptr);
        if (m_ownsKeyStore && m_keyStore->parent() == this) {
            m_keyStore->deleteLater();
        }
    }

    m_keyStore = keyStore;
    m_ownsKeyStore = false;

    if (m_keyStore) {
        connect(m_keyStore, &ApiKeyStore::apiKeyChanged, this, [this]() {
            updateAuthHeaders();
            emit apiKeyChanged();
        });
        connect(m_keyStore, &ApiKeyStore::apiKeySetChanged, this, &AbstractCloudApiClient::apiKeySetChanged);
        updateAuthHeaders();
    }
}

ApiKeyStore* AbstractCloudApiClient::keyStore() const {
    return m_keyStore;
}

QString AbstractCloudApiClient::apiKey() const {
    return m_keyStore ? m_keyStore->apiKey() : QString();
}

void AbstractCloudApiClient::setApiKey(const QString& key) {
    if (m_keyStore) {
        m_keyStore->setApiKey(key);
    }
}

bool AbstractCloudApiClient::apiKeySet() const {
    return m_keyStore ? m_keyStore->apiKeySet() : false;
}

void AbstractCloudApiClient::loadApiKey() {
    if (m_keyStore) {
        m_keyStore->loadApiKey();
    }
}

void AbstractCloudApiClient::ensureApiKeyLoaded() {
    if (m_keyStore) {
        m_keyStore->ensureApiKeyLoaded();
    }
}

void AbstractCloudApiClient::setStorageKeys(const QString& keychainService, const QString& keychainKey,
                                            const QString& settingsKey) {
    if (m_keyStore) {
        m_keyStore->setStorageKeys(keychainService, keychainKey, settingsKey);
    }
}

QNetworkAccessManager* AbstractCloudApiClient::networkAccessManager() {
    return m_nam;
}

const QNetworkRequestFactory& AbstractCloudApiClient::requestFactory() const {
    return m_requestFactory;
}

QNetworkRequest AbstractCloudApiClient::createApiRequest(const QString& relativePath,
                                                         const QString& contentType) const {
    QNetworkRequest request = m_requestFactory.createRequest(relativePath);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setTransferTimeout(m_timeout);
    if (!contentType.isEmpty()) {
        QHttpHeaders headers = request.headers();
        headers.append(QHttpHeaders::WellKnownHeader::ContentType, contentType);
        request.setHeaders(headers);
    }
    return request;
}

QNetworkReply* AbstractCloudApiClient::postJson(const QString& relativePath, const QJsonObject& body,
                                                ResponseCallback callback) {
    QNetworkRequest request = createApiRequest(relativePath, u"application/json"_s);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_nam->post(request, payload);

    connect(reply, &QNetworkReply::finished, this, [reply, timer, callback = std::move(callback)]() {
        reply->deleteLater();
        const qint64 elapsedMs = timer->elapsed();
        const CloudApiResponse response = CloudResponseParser::parseReply(reply, elapsedMs);

        if (callback) {
            callback(response);
        }
    });

    return reply;
}

QNetworkReply* AbstractCloudApiClient::postMultipart(const QString& relativePath, QHttpMultiPart* multiPart,
                                                     ResponseCallback callback) {
    QNetworkRequest request = createApiRequest(relativePath);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    QNetworkReply* reply = m_nam->post(request, multiPart);
    if (multiPart) {
        multiPart->setParent(reply);
    }

    connect(reply, &QNetworkReply::finished, this, [reply, timer, callback = std::move(callback)]() {
        reply->deleteLater();
        const qint64 elapsedMs = timer->elapsed();
        const CloudApiResponse response = CloudResponseParser::parseReply(reply, elapsedMs);

        if (callback) {
            callback(response);
        }
    });

    return reply;
}
