#include "TextInjectorClient.h"

#include "ClipboardManager.h"
#include "DaemonConnector.h"

#include <QMetaObject>
#include <QSettings>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

TextInjectorClient::TextInjectorClient(QObject* parent)
    : QObject(parent)
    , m_clipboard(new ClipboardManager(this))
    , m_connector(new DaemonConnector(this))
    , m_injectionTimer(new QTimer(this)) {
    m_injectionTimer->setSingleShot(true);
    connect(m_injectionTimer, &QTimer::timeout, this, [this]() {
        const QString text = m_pendingText;
        m_pendingText.clear();
        doInjectText(text);
    });

    QSettings settings;
    m_preventClipboardHistory = settings.value(u"Clipboard/PreventHistory"_s, true).toBool();
    m_injectionDelay = settings.value(u"Typing/PreInjectionDelayMs"_s, 200).toInt();
    m_clipboardWarningAcknowledged = settings.value(u"Clipboard/WarningAcknowledged"_s, false).toBool();
    m_clipboardBannerDismissed = settings.value(u"Clipboard/BannerDismissed"_s, false).toBool();

    connect(m_connector, &DaemonConnector::connectedChanged, this, &TextInjectorClient::connectedChanged);
    connect(m_connector, &DaemonConnector::hasFatalErrorChanged, this, &TextInjectorClient::hasFatalErrorChanged);
    connect(m_connector, &DaemonConnector::fatalErrorMessageChanged, this,
            &TextInjectorClient::fatalErrorMessageChanged);
    connect(m_connector, &DaemonConnector::lastErrorChanged, this, [this]() {
        m_lastError = m_connector->lastError();
        emit lastErrorChanged();
    });
    connect(m_connector, &DaemonConnector::statusMessageChanged, this, [this]() {
        m_statusMessage = m_connector->statusMessage();
        emit statusMessageChanged();
    });

    connect(m_clipboard, &ClipboardManager::lastErrorChanged, this, [this](const QString& error) {
        if (!error.isEmpty()) {
            m_lastError = error;
            emit lastErrorChanged();
        }
    });

    QMetaObject::invokeMethod(this, &TextInjectorClient::connectToServer, Qt::QueuedConnection);
}

bool TextInjectorClient::isConnected() const {
    return m_connector && m_connector->isConnected();
}

bool TextInjectorClient::hasFatalError() const {
    return m_connector && m_connector->hasFatalError();
}

QString TextInjectorClient::fatalErrorMessage() const {
    return m_connector ? m_connector->fatalErrorMessage() : QString();
}

QString TextInjectorClient::lastError() const {
    return m_lastError;
}

QString TextInjectorClient::statusMessage() const {
    return m_statusMessage.isEmpty() && m_connector ? m_connector->statusMessage() : m_statusMessage;
}

bool TextInjectorClient::preventClipboardHistory() const {
    return m_preventClipboardHistory;
}

void TextInjectorClient::setPreventClipboardHistory(bool prevent) {
    if (m_preventClipboardHistory != prevent) {
        m_preventClipboardHistory = prevent;
        QSettings settings;
        settings.setValue(u"Clipboard/PreventHistory"_s, prevent);
        emit preventClipboardHistoryChanged();
    }
}

int TextInjectorClient::injectionDelay() const {
    return m_injectionDelay;
}

void TextInjectorClient::setInjectionDelay(int delayMs) {
    delayMs = std::clamp(delayMs, 0, 10000);
    if (m_injectionDelay != delayMs) {
        m_injectionDelay = delayMs;
        QSettings settings;
        settings.setValue(u"Typing/PreInjectionDelayMs"_s, delayMs);
        emit injectionDelayChanged();
    }
}

bool TextInjectorClient::isKde() const {
    const QString xdgCurrentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString xdgSessionDesktop = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    const QString kdeSession = qEnvironmentVariable("KDE_FULL_SESSION");

    if (!kdeSession.isEmpty()) {
        return true;
    }

    if (xdgCurrentDesktop.contains(u"kde"_s, Qt::CaseInsensitive) ||
        xdgCurrentDesktop.contains(u"plasma"_s, Qt::CaseInsensitive) ||
        xdgSessionDesktop.contains(u"kde"_s, Qt::CaseInsensitive) ||
        xdgSessionDesktop.contains(u"plasma"_s, Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

bool TextInjectorClient::clipboardWarningAcknowledged() const {
    return m_clipboardWarningAcknowledged;
}

void TextInjectorClient::setClipboardWarningAcknowledged(bool acknowledged) {
    if (m_clipboardWarningAcknowledged != acknowledged) {
        m_clipboardWarningAcknowledged = acknowledged;
        QSettings settings;
        settings.setValue(u"Clipboard/WarningAcknowledged"_s, acknowledged);
        emit clipboardWarningAcknowledgedChanged();
        emit clipboardWarningRequiredChanged();
    }
}

bool TextInjectorClient::clipboardWarningRequired() const {
    return !isKde() && !m_clipboardWarningAcknowledged;
}

bool TextInjectorClient::clipboardBannerDismissed() const {
    return m_clipboardBannerDismissed;
}

void TextInjectorClient::setClipboardBannerDismissed(bool dismissed) {
    if (m_clipboardBannerDismissed != dismissed) {
        m_clipboardBannerDismissed = dismissed;
        QSettings settings;
        settings.setValue(u"Clipboard/BannerDismissed"_s, dismissed);
        emit clipboardBannerDismissedChanged();
    }
}

void TextInjectorClient::resetClipboardWarning() {
    setClipboardWarningAcknowledged(false);
    setClipboardBannerDismissed(false);
}

void TextInjectorClient::cancelPendingInjection() {
    if (m_injectionTimer && m_injectionTimer->isActive()) {
        m_injectionTimer->stop();
        m_pendingText.clear();
    }
}

void TextInjectorClient::connectToServer() {
    if (m_connector) {
        m_connector->connectToServer();
    }
}

void TextInjectorClient::disconnectFromServer() {
    if (m_connector) {
        m_connector->disconnectFromServer();
    }
}

void TextInjectorClient::stopDaemon() {
    if (m_connector) {
        m_connector->stopDaemon();
    }
}

void TextInjectorClient::restartService() {
    if (m_connector) {
        m_connector->restartService();
    }
}

bool TextInjectorClient::typeText(const QString& text) {
    if (text.isEmpty()) {
        return true;
    }

    if (m_injectionDelay <= 0) {
        cancelPendingInjection();
        return doInjectText(text);
    }

    if (m_injectionTimer->isActive()) {
        m_injectionTimer->stop();
    }

    m_pendingText = text;
    m_injectionTimer->start(std::chrono::milliseconds(m_injectionDelay));
    m_statusMessage = u"Waiting %1 ms before injection…"_s.arg(m_injectionDelay);
    emit statusMessageChanged();
    return true;
}

bool TextInjectorClient::doInjectText(const QString& text) {
    if (text.isEmpty()) {
        return true;
    }

    if (!m_connector->isConnected()) {
        if (!m_connector->connectToServer()) {
            m_lastError = m_connector->lastError();
            emit lastErrorChanged();
            return false;
        }
    }

    // Capture backup only if no restore is currently queued
    QString savedText;
    bool hadContent = false;
    if (!m_clipboard->hasActiveBackup()) {
        savedText = m_clipboard->backupText();
        hadContent = !savedText.isEmpty();
    }

    if (!m_clipboard->setText(text, m_preventClipboardHistory)) {
        m_lastError = m_clipboard->lastError();
        emit lastErrorChanged();
        return false;
    }

    if (!m_connector->sendCommand(keyinjectord::Opcode::Paste)) {
        m_clipboard->restore(savedText, hadContent);
        m_lastError = m_connector->lastError();
        emit lastErrorChanged();
        return false;
    }

    m_statusMessage = u"Injected: \"%1\""_s.arg(text);
    emit statusMessageChanged();
    m_lastError.clear();
    emit lastErrorChanged();

    // Safely restore original clipboard contents after target app processes paste
    m_clipboard->scheduleRestore(savedText, hadContent, 800ms);
    return true;
}
