#include "AbstractSttClient.h"

AbstractSttClient::AbstractSttClient(QObject* parent)
    : QObject(parent) { }

void AbstractSttClient::activate() { }

void AbstractSttClient::deactivate() { }

void AbstractSttClient::retryLast() { }

bool AbstractSttClient::hasNotice() const {
    return false;
}

QVariantMap AbstractSttClient::notice() const {
    return {};
}

QString AbstractSttClient::lastError() const {
    return {};
}
