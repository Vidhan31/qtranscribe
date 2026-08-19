#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QString>

class DaemonConnector : public QObject {
    Q_OBJECT

public:
    explicit DaemonConnector(QObject* parent = nullptr);
    ~DaemonConnector() override;

    bool isConnected() const;
    bool hasFatalError() const;
    QString fatalErrorMessage() const;
    QString lastError() const;
    QString statusMessage() const;

    bool connectToServer();
    void disconnectFromServer();
    void stopDaemon();
    void restartService();

    bool sendCommand(const QByteArray& cmdJson);
    bool ensureDaemonRunning();
    QString socketPath() const;

signals:
    void connectedChanged();
    void hasFatalErrorChanged();
    void fatalErrorMessageChanged();
    void lastErrorChanged();
    void statusMessageChanged();

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QLocalSocket::LocalSocketError error);
    void onReadyRead();

private:
    void setLastError(const QString& error);
    void setStatusMessage(const QString& message);
    void setFatalError(bool fatal, const QString& msg = QString());

    QLocalSocket* m_socket = nullptr;
    QProcess* m_daemonProcess = nullptr;
    bool m_hasFatalError = false;
    QString m_fatalErrorMessage;
    QString m_lastError;
    QString m_statusMessage;
};
