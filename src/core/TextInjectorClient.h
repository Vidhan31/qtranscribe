#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class ClipboardManager;
class DaemonConnector;
class QTimer;

class TextInjectorClient : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(bool hasFatalError READ hasFatalError NOTIFY hasFatalErrorChanged FINAL)
    Q_PROPERTY(QString fatalErrorMessage READ fatalErrorMessage NOTIFY fatalErrorMessageChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
    Q_PROPERTY(bool preventClipboardHistory READ preventClipboardHistory WRITE setPreventClipboardHistory NOTIFY preventClipboardHistoryChanged FINAL)
    Q_PROPERTY(int injectionDelay READ injectionDelay WRITE setInjectionDelay NOTIFY injectionDelayChanged FINAL)
    Q_PROPERTY(bool isKde READ isKde CONSTANT FINAL)
    Q_PROPERTY(bool clipboardWarningAcknowledged READ clipboardWarningAcknowledged WRITE setClipboardWarningAcknowledged NOTIFY clipboardWarningAcknowledgedChanged FINAL)
    Q_PROPERTY(bool clipboardWarningRequired READ clipboardWarningRequired NOTIFY clipboardWarningRequiredChanged FINAL)
    Q_PROPERTY(bool clipboardBannerDismissed READ clipboardBannerDismissed WRITE setClipboardBannerDismissed NOTIFY clipboardBannerDismissedChanged FINAL)

public:
    explicit TextInjectorClient(QObject* parent = nullptr);
    ~TextInjectorClient() override = default;

    bool isConnected() const;
    bool hasFatalError() const;
    QString fatalErrorMessage() const;
    QString lastError() const;
    QString statusMessage() const;

    bool preventClipboardHistory() const;
    void setPreventClipboardHistory(bool prevent);

    int injectionDelay() const;
    void setInjectionDelay(int delayMs);

    bool isKde() const;
    bool clipboardWarningAcknowledged() const;
    void setClipboardWarningAcknowledged(bool acknowledged);
    bool clipboardWarningRequired() const;
    bool clipboardBannerDismissed() const;
    void setClipboardBannerDismissed(bool dismissed);

    Q_INVOKABLE bool typeText(const QString& text);
    Q_INVOKABLE void cancelPendingInjection();
    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void stopDaemon();
    Q_INVOKABLE void restartService();
    Q_INVOKABLE void resetClipboardWarning();

signals:
    void connectedChanged();
    void hasFatalErrorChanged();
    void fatalErrorMessageChanged();
    void lastErrorChanged();
    void statusMessageChanged();
    void preventClipboardHistoryChanged();
    void injectionDelayChanged();
    void clipboardWarningAcknowledgedChanged();
    void clipboardWarningRequiredChanged();
    void clipboardBannerDismissedChanged();

private:
    bool doInjectText(const QString& text);

    ClipboardManager* m_clipboard = nullptr;
    DaemonConnector* m_connector = nullptr;
    QTimer* m_injectionTimer = nullptr;
    QString m_pendingText;
    QString m_lastError;
    QString m_statusMessage;
    int m_injectionDelay = 200;
    bool m_preventClipboardHistory = true;
    bool m_clipboardWarningAcknowledged = false;
    bool m_clipboardBannerDismissed = false;
};
