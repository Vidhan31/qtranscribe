#pragma once

#include "AbstractCloudApiClient.h"

#include <QQmlEngine>
#include <QStringView>

class GeminiApiClient : public AbstractCloudApiClient {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit GeminiApiClient(QObject* parent = nullptr);
    ~GeminiApiClient() override = default;

    static constexpr auto kDefaultTransferTimeout = std::chrono::seconds(20);
    inline static constexpr QStringView kApiBaseUrl = u"https://generativelanguage.googleapis.com/v1beta";
    inline static constexpr QStringView kKeychainKey = u"gemini_api_key";
    inline static constexpr QStringView kSettingsApiKey = u"Cloud/Providers/Gemini/ApiKey";

protected:
    void updateAuthHeaders() override;
};
