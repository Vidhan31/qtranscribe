#include "AbstractSttClient.h"

AbstractSttClient::AbstractSttClient(QObject* parent)
    : QObject(parent) { }

void AbstractSttClient::activate() { }

void AbstractSttClient::deactivate() { }

bool AbstractSttClient::hasNotice() const {
    return false;
}

QVariantMap AbstractSttClient::notice() const {
    return {};
}
