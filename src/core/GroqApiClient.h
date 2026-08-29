#pragma once

#include "AbstractCloudApiClient.h"

#include <QQmlEngine>
#include <QStringView>

class GroqApiClient : public AbstractCloudApiClient {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit GroqApiClient(QObject* parent = nullptr);
    ~GroqApiClient() override = default;

    static constexpr auto kDefaultTransferTimeout = std::chrono::seconds(15);
    inline static constexpr QStringView kApiBaseUrl = u"https://api.groq.com/openai/v1";

protected:
    void updateAuthHeaders() override;
};
