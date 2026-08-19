#include "DaemonConnector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QStandardPaths>
#include <QThread>

#include <unistd.h>

using namespace Qt::StringLiterals;

DaemonConnector::DaemonConnector(QObject* parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this)) {
    connect(m_socket, &QLocalSocket::connected, this, &DaemonConnector::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DaemonConnector::onDisconnected);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &DaemonConnector::onErrorOccurred);
    connect(m_socket, &QLocalSocket::readyRead, this, &DaemonConnector::onReadyRead);

    connect(qApp, &QCoreApplication::aboutToQuit, this, &DaemonConnector::stopDaemon);
}

DaemonConnector::~DaemonConnector() {
    stopDaemon();
}

bool DaemonConnector::isConnected() const {
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState;
}

bool DaemonConnector::hasFatalError() const {
    return m_hasFatalError;
}

QString DaemonConnector::fatalErrorMessage() const {
    return m_fatalErrorMessage;
}

QString DaemonConnector::lastError() const {
    return m_lastError;
}

QString DaemonConnector::statusMessage() const {
    return m_statusMessage;
}

QString DaemonConnector::socketPath() const {
    QString xdgRuntime = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    if (!xdgRuntime.isEmpty()) {
        return xdgRuntime + u"/keyinjectord.sock"_s;
    }

    // Secure fallback matching keyinjectord daemon path
    QString userDir = u"/tmp/qtranscribe-%1"_s.arg(getuid());
    QDir().mkdir(userDir);
    QFile::setPermissions(userDir, QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser);
    return userDir + u"/keyinjectord.sock"_s;
}

bool DaemonConnector::ensureDaemonRunning() {
    {
        QLocalSocket testSocket;
        testSocket.connectToServer(socketPath());
        if (testSocket.waitForConnected(200)) {
            testSocket.disconnectFromServer();
            return true;
        }
    }

    if (m_daemonProcess && m_daemonProcess->state() != QProcess::NotRunning) {
        return true;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidatePaths = {appDir + u"/keyinjectord"_s,
                                        appDir + u"/src/keyinjectord/keyinjectord"_s,
                                        appDir + u"/keyinjectord/keyinjectord"_s,
                                        appDir + u"/../build-keyinjectord/keyinjectord"_s,
                                        QDir::currentPath() + u"/build-keyinjectord/keyinjectord"_s,
                                        QStandardPaths::findExecutable(u"keyinjectord"_s)};

    QString lastCapturedError;

    for (const QString& daemonExecutable : candidatePaths) {
        if (daemonExecutable.isEmpty() || !QFile::exists(daemonExecutable)) {
            continue;
        }

        setStatusMessage(u"Starting keyinjectord (%1)..."_s.arg(daemonExecutable));

        if (!m_daemonProcess) {
            m_daemonProcess = new QProcess(this);
        }

        m_daemonProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_daemonProcess->start(daemonExecutable, {});

        if (!m_daemonProcess->waitForStarted(2000)) {
            lastCapturedError = u"Failed to launch %1: %2"_s.arg(daemonExecutable, m_daemonProcess->errorString());
            delete m_daemonProcess;
            m_daemonProcess = nullptr;
            continue;
        }

        bool success = false;
        for (int retry = 0; retry < 25; ++retry) {
            QThread::msleep(100);

            if (m_daemonProcess->state() == QProcess::NotRunning) {
                QByteArray output = m_daemonProcess->readAll();
                lastCapturedError = QString::fromUtf8(output).trimmed();
                if (lastCapturedError.isEmpty()) {
                    lastCapturedError = daemonExecutable + u" exited immediately with exit code "_s +
                                        QString::number(m_daemonProcess->exitCode());
                }
                delete m_daemonProcess;
                m_daemonProcess = nullptr;
                break;
            }

            QLocalSocket testSocket;
            testSocket.connectToServer(socketPath());
            if (testSocket.waitForConnected(100)) {
                testSocket.disconnectFromServer();
                success = true;
                break;
            }
        }

        if (success) {
            setFatalError(false);
            return true;
        }

        if (m_daemonProcess) {
            m_daemonProcess->terminate();
            delete m_daemonProcess;
            m_daemonProcess = nullptr;
        }
    }

    if (!lastCapturedError.isEmpty()) {
        setLastError(lastCapturedError);
        setFatalError(true, lastCapturedError);
    } else {
        const QString notFoundErr = u"keyinjectord binary not found or failed to start."_s;
        setLastError(notFoundErr);
        setFatalError(true, notFoundErr);
    }
    return false;
}

bool DaemonConnector::connectToServer() {
    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        setStatusMessage(u"Already connected or connecting"_s);
        return true;
    }

    if (!ensureDaemonRunning()) {
        return false;
    }

    QString path = socketPath();
    setStatusMessage(u"Connecting to %1..."_s.arg(path));
    setLastError({});

    m_socket->connectToServer(path);
    return true;
}

void DaemonConnector::disconnectFromServer() {
    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_socket->disconnectFromServer();
    }
}

void DaemonConnector::stopDaemon() {
    if (m_daemonProcess && m_daemonProcess->state() != QProcess::NotRunning) {
        setStatusMessage(u"Stopping keyinjectord daemon..."_s);
        m_daemonProcess->terminate();
        if (!m_daemonProcess->waitForFinished(1000)) {
            m_daemonProcess->kill();
        }
        delete m_daemonProcess;
        m_daemonProcess = nullptr;
    }
}

void DaemonConnector::restartService() {
    stopDaemon();
    disconnectFromServer();
    connectToServer();
}

bool DaemonConnector::sendCommand(const QByteArray& cmdJson) {
    if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState) {
        setLastError(u"Socket not connected"_s);
        return false;
    }

    qint64 written = m_socket->write(cmdJson);
    if (written != cmdJson.size()) {
        setLastError(u"Failed to write full command to socket"_s);
        return false;
    }
    m_socket->flush();
    return true;
}

void DaemonConnector::onConnected() {
    setStatusMessage(u"Connected to keyinjectord"_s);
    setLastError({});
    setFatalError(false);
    emit connectedChanged();
}

void DaemonConnector::onDisconnected() {
    setStatusMessage(u"Disconnected from keyinjectord"_s);
    emit connectedChanged();
}

void DaemonConnector::onErrorOccurred(QLocalSocket::LocalSocketError error) {
    Q_UNUSED(error)
    QString msg = m_socket->errorString();
    if (error == QLocalSocket::ServerNotFoundError || error == QLocalSocket::ConnectionRefusedError) {
        msg = u"keyinjectord is not running."_s;
    }
    setLastError(msg);
    emit connectedChanged();
}

void DaemonConnector::onReadyRead() {
    QByteArray data = m_socket->readAll();
    QString response = QString::fromUtf8(data).trimmed();
    if (!response.isEmpty()) {
        setStatusMessage(u"Response: %1"_s.arg(response));
    }
}

void DaemonConnector::setFatalError(bool fatal, const QString& msg) {
    bool changed = false;
    if (m_hasFatalError != fatal) {
        m_hasFatalError = fatal;
        emit hasFatalErrorChanged();
        changed = true;
    }
    if (m_fatalErrorMessage != msg) {
        m_fatalErrorMessage = msg;
        emit fatalErrorMessageChanged();
        changed = true;
    }
    if (changed) {
        emit connectedChanged();
    }
}

void DaemonConnector::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}

void DaemonConnector::setStatusMessage(const QString& message) {
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
}
