#include "GroqApiClient.h"

#include "LoggingCategories.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHttpHeaders>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxyFactory>
#include <QSslConfiguration>
#include <QSslSocket>

#include <memory>

using namespace Qt::StringLiterals;

GroqApiClient::GroqApiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this)) {
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Enforce modern TLS security (TLS 1.2+ minimum, ALPN HTTP/2 negotiation, strict peer verification)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);
    sslConfig.setAllowedNextProtocols({QSslConfiguration::ALPNProtocolHTTP2});
    QSslConfiguration::setDefaultConfiguration(sslConfig);

    connect(m_nam, &QNetworkAccessManager::sslErrors, this,
            [](QNetworkReply* /*reply*/, const QList<QSslError>& errors) {
                for (const auto& err : errors) {
                    qWarning() << "GroqApiClient SSL Error:" << err.errorString();
                }
            });

    m_nam->setTransferTimeout(kDefaultTransferTimeout);

    m_requestFactory.setBaseUrl(QUrl(kApiBaseUrl.toString()));
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::UserAgent, userAgent());
    m_requestFactory.setCommonHeaders(headers);

    loadApiKeyFromKeychain();
}

#ifndef QTRANSCRIBE_VERSION
#define QTRANSCRIBE_VERSION "0.0.0-dev"
#endif

QString GroqApiClient::userAgent() {
    const QString appVer = QCoreApplication::applicationVersion().isEmpty() ? u"" QTRANSCRIBE_VERSION ""_s
                                                                            : QCoreApplication::applicationVersion();
    return u"QTranscribe/%1 (Linux; Qt %2)"_s.arg(appVer, QString::fromUtf8(QT_VERSION_STR));
}

QString GroqApiClient::apiKey() const {
    return m_apiKey;
}

void GroqApiClient::setApiKey(const QString& key) {
    const QString trimmed = key.trimmed();
    if (m_apiKey != trimmed) {
        const bool wasSet = apiKeySet();
        m_apiKey = trimmed;
        updateFactoryAuth();
        emit apiKeyChanged();

        if (wasSet != apiKeySet()) {
            emit apiKeySetChanged();
        }

        if (!m_apiKey.isEmpty()) {
            saveApiKeyToKeychain(m_apiKey);
        } else {
            deleteApiKeyFromKeychain();
        }
    }
}

void GroqApiClient::updateFactoryAuth() {
    if (!m_apiKey.isEmpty()) {
        m_requestFactory.setBearerToken(m_apiKey.toUtf8());
    } else {
        m_requestFactory.setBearerToken(QByteArray());
    }
}

bool GroqApiClient::apiKeySet() const {
    return !m_apiKey.isEmpty();
}

QNetworkAccessManager* GroqApiClient::networkAccessManager() {
    return m_nam;
}

const QNetworkRequestFactory& GroqApiClient::requestFactory() const {
    return m_requestFactory;
}

QNetworkRequest GroqApiClient::createApiRequest(const QString& relativePath, const QString& contentType) const {
    QNetworkRequest request = m_requestFactory.createRequest(relativePath);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setTransferTimeout(kDefaultTransferTimeout);
    if (!contentType.isEmpty()) {
        QHttpHeaders headers = request.headers();
        headers.append(QHttpHeaders::WellKnownHeader::ContentType, contentType);
        request.setHeaders(headers);
    }
    return request;
}

QNetworkReply* GroqApiClient::postJson(const QString& relativePath, const QJsonObject& body,
                                       const QString& endpointLabel, const QString& modelName,
                                       ResponseCallback callback) {
    QNetworkRequest request = createApiRequest(relativePath, u"application/json"_s);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_nam->post(request, payload);

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, endpointLabel, modelName, callback]() {
        reply->deleteLater();
        const qint64 elapsedMs = timer->elapsed();
        const GroqApiResponse response = GroqResponseParser::parseReply(reply, elapsedMs);

        emit responseProcessed(reply, endpointLabel, modelName, elapsedMs, response.rawBody);

        if (callback) {
            callback(response);
        }
    });

    return reply;
}

QNetworkReply* GroqApiClient::postMultipart(const QString& relativePath, QHttpMultiPart* multiPart,
                                            const QString& endpointLabel, const QString& modelName,
                                            ResponseCallback callback) {
    QNetworkRequest request = createApiRequest(relativePath);
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    QNetworkReply* reply = m_nam->post(request, multiPart);
    if (multiPart) {
        multiPart->setParent(reply);
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, endpointLabel, modelName, callback]() {
        reply->deleteLater();
        const qint64 elapsedMs = timer->elapsed();
        const GroqApiResponse response = GroqResponseParser::parseReply(reply, elapsedMs);

        emit responseProcessed(reply, endpointLabel, modelName, elapsedMs, response.rawBody);

        if (callback) {
            callback(response);
        }
    });

    return reply;
}

void GroqApiClient::loadApiKeyFromKeychain() {
    auto* readJob = new QKeychain::ReadPasswordJob(kKeychainService.toString(), this);
    readJob->setKey(kKeychainKey.toString());
    readJob->setAutoDelete(true);

    connect(readJob, &QKeychain::ReadPasswordJob::finished, this, [this](QKeychain::Job* job) {
        auto* rJob = static_cast<QKeychain::ReadPasswordJob*>(job);
        if (!rJob->error()) {
            const QString key = rJob->textData().trimmed();
            if (!key.isEmpty()) {
                qCDebug(lcNetwork) << "Groq API key loaded successfully from system keychain (key length:" << key.size()
                                   << ")";
                m_apiKey = key;
                updateFactoryAuth();
                emit apiKeyChanged();
                emit apiKeySetChanged();
            }
        } else if (rJob->error() == QKeychain::EntryNotFound) {
            qCDebug(lcNetwork) << "No Groq API key found in system keychain";
        } else {
            qWarning("GroqApiClient: Failed to read API key from keychain: %s", qPrintable(rJob->errorString()));
        }
    });

    readJob->start();
}

void GroqApiClient::saveApiKeyToKeychain(const QString& key) {
    auto* writeJob = new QKeychain::WritePasswordJob(kKeychainService.toString(), this);
    writeJob->setKey(kKeychainKey.toString());
    writeJob->setTextData(key);
    writeJob->setAutoDelete(true);

    connect(writeJob, &QKeychain::WritePasswordJob::finished, this, [](QKeychain::Job* job) {
        if (job->error()) {
            qWarning("GroqApiClient: Failed to save API key to keychain: %s", qPrintable(job->errorString()));
        } else {
            qCDebug(lcNetwork) << "Groq API key persisted into system keychain";
        }
    });

    writeJob->start();
}

void GroqApiClient::deleteApiKeyFromKeychain() {
    auto* deleteJob = new QKeychain::DeletePasswordJob(kKeychainService.toString(), this);
    deleteJob->setKey(kKeychainKey.toString());
    deleteJob->setAutoDelete(true);

    connect(deleteJob, &QKeychain::DeletePasswordJob::finished, this, [](QKeychain::Job* job) {
        if (job->error() && job->error() != QKeychain::EntryNotFound) {
            qWarning("GroqApiClient: Failed to delete API key from keychain: %s", qPrintable(job->errorString()));
        } else {
            qCDebug(lcNetwork) << "Groq API key deleted from system keychain";
        }
    });

    deleteJob->start();
}
