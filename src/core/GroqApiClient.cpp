#include "GroqApiClient.h"

#include <QUrl>

GroqApiClient::GroqApiClient(QObject* parent)
    : AbstractCloudApiClient(QUrl(kApiBaseUrl.toString()), kDefaultTransferTimeout, parent) {
    updateAuthHeaders();
}

void GroqApiClient::updateAuthHeaders() {
    const QString key = apiKey();
    if (!key.isEmpty()) {
        m_requestFactory.setBearerToken(key.toUtf8());
    } else {
        m_requestFactory.setBearerToken(QByteArray());
    }
}
