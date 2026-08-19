#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class GroqApiClient;
class QNetworkReply;

class GroqUsageTracker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasData READ hasData NOTIFY dataChanged FINAL)
    Q_PROPERTY(qint64 limitRequests READ limitRequests NOTIFY dataChanged FINAL)
    Q_PROPERTY(qint64 remainingRequests READ remainingRequests NOTIFY dataChanged FINAL)
    Q_PROPERTY(QString resetRequests READ resetRequests NOTIFY dataChanged FINAL)
    Q_PROPERTY(qreal requestsUsageFraction READ requestsUsageFraction NOTIFY dataChanged FINAL)

    Q_PROPERTY(qint64 limitTokens READ limitTokens NOTIFY dataChanged FINAL)
    Q_PROPERTY(qint64 remainingTokens READ remainingTokens NOTIFY dataChanged FINAL)
    Q_PROPERTY(QString resetTokens READ resetTokens NOTIFY dataChanged FINAL)
    Q_PROPERTY(qreal tokensUsageFraction READ tokensUsageFraction NOTIFY dataChanged FINAL)

    Q_PROPERTY(bool hasAudioSecondsLimit READ hasAudioSecondsLimit NOTIFY dataChanged FINAL)
    Q_PROPERTY(qint64 limitAudioSeconds READ limitAudioSeconds NOTIFY dataChanged FINAL)
    Q_PROPERTY(qint64 remainingAudioSeconds READ remainingAudioSeconds NOTIFY dataChanged FINAL)
    Q_PROPERTY(QString resetAudioSeconds READ resetAudioSeconds NOTIFY dataChanged FINAL)

    Q_PROPERTY(bool isRateLimited READ isRateLimited NOTIFY dataChanged FINAL)
    Q_PROPERTY(int retryAfterSeconds READ retryAfterSeconds NOTIFY dataChanged FINAL)

    Q_PROPERTY(int sessionTotalRequests READ sessionTotalRequests NOTIFY sessionStatsChanged FINAL)
    Q_PROPERTY(int sessionSttRequests READ sessionSttRequests NOTIFY sessionStatsChanged FINAL)
    Q_PROPERTY(int sessionLlmRequests READ sessionLlmRequests NOTIFY sessionStatsChanged FINAL)
    Q_PROPERTY(qint64 sessionPromptTokens READ sessionPromptTokens NOTIFY sessionStatsChanged FINAL)
    Q_PROPERTY(qint64 sessionCompletionTokens READ sessionCompletionTokens NOTIFY sessionStatsChanged FINAL)
    Q_PROPERTY(qint64 sessionTotalTokens READ sessionTotalTokens NOTIFY sessionStatsChanged FINAL)

    Q_PROPERTY(QString lastEndpoint READ lastEndpoint NOTIFY lastRequestChanged FINAL)
    Q_PROPERTY(QString lastModel READ lastModel NOTIFY lastRequestChanged FINAL)
    Q_PROPERTY(int lastHttpStatus READ lastHttpStatus NOTIFY lastRequestChanged FINAL)
    Q_PROPERTY(qint64 lastLatencyMs READ lastLatencyMs NOTIFY lastRequestChanged FINAL)
    Q_PROPERTY(QString lastUpdatedTimestamp READ lastUpdatedTimestamp NOTIFY lastRequestChanged FINAL)

    Q_PROPERTY(bool checkingQuota READ checkingQuota NOTIFY checkingQuotaChanged FINAL)
    Q_PROPERTY(QString quotaCheckError READ quotaCheckError NOTIFY quotaCheckErrorChanged FINAL)

public:
    explicit GroqUsageTracker(QObject* parent = nullptr);
    ~GroqUsageTracker() override = default;

    void setApiClient(GroqApiClient* client);
    GroqApiClient* apiClient() const;

    bool hasData() const;
    qint64 limitRequests() const;
    qint64 remainingRequests() const;
    QString resetRequests() const;
    qreal requestsUsageFraction() const;

    qint64 limitTokens() const;
    qint64 remainingTokens() const;
    QString resetTokens() const;
    qreal tokensUsageFraction() const;

    bool hasAudioSecondsLimit() const;
    qint64 limitAudioSeconds() const;
    qint64 remainingAudioSeconds() const;
    QString resetAudioSeconds() const;

    bool isRateLimited() const;
    int retryAfterSeconds() const;

    int sessionTotalRequests() const;
    int sessionSttRequests() const;
    int sessionLlmRequests() const;
    qint64 sessionPromptTokens() const;
    qint64 sessionCompletionTokens() const;
    qint64 sessionTotalTokens() const;

    QString lastEndpoint() const;
    QString lastModel() const;
    int lastHttpStatus() const;
    qint64 lastLatencyMs() const;
    QString lastUpdatedTimestamp() const;

    bool checkingQuota() const;
    QString quotaCheckError() const;

    void recordResponse(QNetworkReply* reply, const QString& endpointType, const QString& model, qint64 latencyMs,
                        const QByteArray& responseBody);

    Q_INVOKABLE void refreshQuota();
    Q_INVOKABLE void resetSessionStats();

signals:
    void dataChanged();
    void sessionStatsChanged();
    void lastRequestChanged();
    void checkingQuotaChanged();
    void quotaCheckErrorChanged();

private:
    void loadFromSettings();
    void saveToSettings();
    void setCheckingQuota(bool checking);
    void setQuotaCheckError(const QString& error);

    GroqApiClient* m_apiClient = nullptr;

    bool m_hasData = false;
    qint64 m_limitRequests = 0;
    qint64 m_remainingRequests = 0;
    QString m_resetRequests;

    qint64 m_limitTokens = 0;
    qint64 m_remainingTokens = 0;
    QString m_resetTokens;

    bool m_hasAudioSecondsLimit = false;
    qint64 m_limitAudioSeconds = 0;
    qint64 m_remainingAudioSeconds = 0;
    QString m_resetAudioSeconds;

    bool m_isRateLimited = false;
    int m_retryAfterSeconds = 0;

    int m_sessionTotalRequests = 0;
    int m_sessionSttRequests = 0;
    int m_sessionLlmRequests = 0;
    qint64 m_sessionPromptTokens = 0;
    qint64 m_sessionCompletionTokens = 0;
    qint64 m_sessionTotalTokens = 0;

    QString m_lastEndpoint;
    QString m_lastModel;
    int m_lastHttpStatus = 0;
    qint64 m_lastLatencyMs = 0;
    QString m_lastUpdatedTimestamp;

    bool m_checkingQuota = false;
    QString m_quotaCheckError;
};
