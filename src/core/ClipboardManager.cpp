#include "ClipboardManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <array>
#include <ranges>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

ClipboardManager::ClipboardManager(QObject* parent)
    : QObject(parent)
    , m_restoreTimer(new QTimer(this)) {
    m_restoreTimer->setSingleShot(true);
    connect(m_restoreTimer, &QTimer::timeout, this, [this]() {
        if (m_hasActiveBackup) {
            restore(m_savedClipboardText, m_hadClipboardContent);
            m_hasActiveBackup = false;
            m_savedClipboardText.clear();
        }
    });
}

ClipboardManager::~ClipboardManager() {
    if (m_hasActiveBackup) {
        m_restoreTimer->stop();
        restore(m_savedClipboardText, m_hadClipboardContent);
    }
}

QString ClipboardManager::lastError() const {
    return m_lastError;
}

void ClipboardManager::setLastError(const QString& error) {
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged(m_lastError);
    }
}

bool ClipboardManager::hasActiveBackup() const {
    return m_hasActiveBackup;
}

void ClipboardManager::cancelPendingRestore() {
    if (m_restoreTimer->isActive()) {
        m_restoreTimer->stop();
    }
    m_hasActiveBackup = false;
    m_savedClipboardText.clear();
}

void ClipboardManager::scheduleRestore(const QString& backupText, bool hadContent, std::chrono::milliseconds delay) {
    m_savedClipboardText = backupText;
    m_hadClipboardContent = hadContent;
    m_hasActiveBackup = true;
    m_restoreTimer->start(delay);
}

QString ClipboardManager::bundledWlToolPath(const QString& toolName) const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const auto searchLocations = std::to_array<QString>(
        {appDir + u"/"_s + toolName, appDir + u"/../libexec/"_s + toolName, appDir + u"/libexec/"_s + toolName,
         u"/usr/lib/qtranscribe/"_s + toolName, u"/usr/libexec/qtranscribe/"_s + toolName});

    const auto it = std::ranges::find_if(searchLocations, [](const QString& candidate) {
        const QFileInfo fi(candidate);
        return fi.exists() && fi.isExecutable();
    });
    return it != searchLocations.end() ? *it : toolName;
}

QString ClipboardManager::backupText() const {
    QString wlPaste = bundledWlToolPath(u"wl-paste"_s);
    QProcess proc;
    proc.start(wlPaste, {u"--no-newline"_s, u"--type"_s, u"text/plain"_s});
    if (!proc.waitForFinished(500) || proc.exitCode() != 0) {
        return {};
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

bool ClipboardManager::setText(const QString& text, bool sensitive) {
    QString wlCopy = bundledWlToolPath(u"wl-copy"_s);
    QProcess proc;
    QStringList args;
    if (sensitive) {
        args.append(u"--sensitive"_s);
    }
    args.append(u"--type"_s);
    args.append(u"text/plain"_s);
    proc.start(wlCopy, args);
    if (!proc.waitForStarted(1000)) {
        setLastError(u"Failed to start wl-copy (%1). Is wl-clipboard built/installed?"_s.arg(wlCopy));
        return false;
    }
    proc.write(text.toUtf8());
    proc.closeWriteChannel();
    if (!proc.waitForFinished(1000)) {
        setLastError(u"wl-copy operation timed out"_s);
        return false;
    }
    if (proc.exitCode() != 0) {
        setLastError(u"wl-copy failed with exit code %1"_s.arg(proc.exitCode()));
        return false;
    }
    setLastError({});
    return true;
}

void ClipboardManager::restore(const QString& backup, bool hadContent) {
    QString wlCopy = bundledWlToolPath(u"wl-copy"_s);
    if (!hadContent) {
        QProcess proc;
        proc.start(wlCopy, {u"--clear"_s});
        proc.waitForFinished(500);
        return;
    }
    setText(backup, /*sensitive=*/false);
}
