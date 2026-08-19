#pragma once

#include "GroqResponseParser.h"

#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>

class ApiRequestHandler : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)

public:
    struct RetryPolicy {
        int maxRetries = 1;
        int defaultDelayMs = 1000;
        int maxTransientRetryAfterSec = 3;
    };

    explicit ApiRequestHandler(QObject* parent = nullptr);
    ~ApiRequestHandler() override;

    bool isBusy() const;
    bool isCancelled() const;

    virtual void cancel();

signals:
    void busyChanged();

protected:
    void setBusy(bool busy);
    void setCurrentReply(QNetworkReply* reply);
    void resetRequestState();
    void prepareNewRequest();

    bool shouldRetry(const GroqApiResponse& res) const;
    int calculateRetryDelayMs(const GroqApiResponse& res) const;

    RetryPolicy m_retryPolicy;
    QPointer<QNetworkReply> m_currentReply;
    int m_retryCount = 0;
    bool m_busy = false;
    bool m_cancelled = false;
};
