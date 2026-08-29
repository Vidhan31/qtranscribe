#include "GeminiApiClient.h"

#include "ApiKeyStore.h"

#include <QHttpHeaders>
#include <QUrl>

using namespace Qt::StringLiterals;

GeminiApiClient::GeminiApiClient(QObject* parent)
    : AbstractCloudApiClient(QUrl(kApiBaseUrl.toString()), kDefaultTransferTimeout, parent) {
    if (m_keyStore) {
        m_keyStore->setStorageKeys(u"QTranscribe"_s, kKeychainKey.toString(), kSettingsApiKey.toString());
    }
    updateAuthHeaders();
}

void GeminiApiClient::updateAuthHeaders() {
    QHttpHeaders headers = m_requestFactory.commonHeaders();
    headers.removeAll(QByteArrayView("x-goog-api-key"));
    const QString key = apiKey();
    if (!key.isEmpty()) {
        headers.append(QByteArrayView("x-goog-api-key"), key.toUtf8());
    }
    m_requestFactory.setCommonHeaders(headers);
}
