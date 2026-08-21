#pragma once

#include "HttpRequestRunner.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

class GroqApiClient;
class QTimer;

class GroqLlmClient : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(QString activePreset READ activePreset WRITE setActivePreset NOTIFY activePresetChanged FINAL)
    Q_PROPERTY(QString customPrompt READ customPrompt WRITE setCustomPrompt NOTIFY customPromptChanged FINAL)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY temperatureChanged FINAL)

public:
    explicit GroqLlmClient(QObject* parent = nullptr);
    ~GroqLlmClient() override = default;

    void setApiClient(GroqApiClient* apiClient);
    GroqApiClient* apiClient() const;

    bool isBusy() const;
    bool isCancelled() const;

    bool enabled() const;
    void setEnabled(bool enabled);

    QString lastError() const;

    QString selectedModel() const;
    void setSelectedModel(const QString& model);

    QString activePreset() const;
    void setActivePreset(const QString& preset);

    QString customPrompt() const;
    void setCustomPrompt(const QString& prompt);

    double temperature() const;
    void setTemperature(double temp);

    Q_INVOKABLE QString systemPromptForPreset(const QString& preset) const;
    Q_INVOKABLE void processText(const QString& rawText);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void enabledChanged();
    void lastErrorChanged();
    void selectedModelChanged();
    void activePresetChanged();
    void customPromptChanged();
    void temperatureChanged();

    void enhancementReady(const QString& enhancedText);
    void errorOccurred(const QString& error, const QString& fallbackRawText);

private:
    void setBusy(bool busy);
    void setLastError(const QString& error);
    void sendProcessRequest();
    void handleProcessResponse(const GroqApiResponse& res);
    QString currentSystemPrompt() const;

    GroqApiClient* m_apiClient = nullptr;
    QTimer* m_retryTimer = nullptr;
    HttpRequestRunner m_requestRunner;
    QString m_lastError;
    QString m_selectedModel;
    QString m_activePreset;
    QString m_customPrompt;
    QString m_pendingRawText;
    double m_temperature = 0.1;
    bool m_enabled = false;

    inline static constexpr QStringView kDefaultModel = u"openai/gpt-oss-20b";
};
