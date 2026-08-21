#include "GlobalShortcutManager.h"

#include "LoggingCategories.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

QDBusArgument& operator<<(QDBusArgument& argument, const PortalShortcut& shortcut) {
    argument.beginStructure();
    argument << shortcut.id << shortcut.options;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalShortcut& shortcut) {
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.options;
    argument.endStructure();
    return argument;
}

GlobalShortcutManager::GlobalShortcutManager(QObject* parent)
    : QObject(parent) {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<ShortcutList>();

    quint32 randVal = QRandomGenerator::global()->generate();
    m_handleToken = u"stt_req_%1"_s.arg(randVal);
    m_sessionHandleToken = u"stt_sess_%1"_s.arg(randVal);

    qCDebug(lcShortcut) << "GlobalShortcutManager initialized -> Token:" << m_handleToken
                        << "SessionToken:" << m_sessionHandleToken;

    QTimer::singleShot(50ms, this, &GlobalShortcutManager::initializePortal);
}

bool GlobalShortcutManager::isAvailable() const {
    return m_available;
}

bool GlobalShortcutManager::isSupported() const {
    return m_supported;
}

QString GlobalShortcutManager::statusMessage() const {
    return m_statusMessage;
}

void GlobalShortcutManager::setAvailable(bool available) {
    if (m_available != available) {
        m_available = available;
        emit availableChanged();
    }
}

void GlobalShortcutManager::setSupported(bool supported) {
    if (m_supported != supported) {
        m_supported = supported;
        emit supportedChanged();
    }
}

void GlobalShortcutManager::setStatusMessage(const QString& msg) {
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void GlobalShortcutManager::requestShortcuts() {
    m_sessionRequested = false;
    initializePortal();
}

namespace {
const auto kApplicationId = u"io.github.qtranscribe"_s;
const auto kDesktopFileName = u"io.github.qtranscribe.desktop"_s;
const auto kLegacyDesktopFileName = u"qtranscribe.desktop"_s;
} // namespace

void GlobalShortcutManager::ensureIconExists() {
    const QString iconsBaseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (iconsBaseDir.isEmpty()) {
        return;
    }

    const QString userIconsDir = iconsBaseDir + u"/icons/hicolor/128x128/apps"_s;
    QDir dir(userIconsDir);
    if (!dir.exists()) {
        dir.mkpath(u"."_s);
    }

    const QString iconPath = dir.filePath(u"io.github.qtranscribe.png"_s);
    if (!QFile::exists(iconPath)) {
        if (QFile::copy(u":/qt/qml/QTranscribe/assets/speech-to-text-128.png"_s, iconPath)) {
            qCDebug(lcShortcut) << "Exported app icon to:" << iconPath;
        }
    }

    // Also write legacy icon name for fallback compatibility
    const QString legacyIconPath = dir.filePath(u"qtranscribe.png"_s);
    if (!QFile::exists(legacyIconPath)) {
        QFile::copy(u":/qt/qml/QTranscribe/assets/speech-to-text-128.png"_s, legacyIconPath);
    }
}

void GlobalShortcutManager::ensureDesktopFileExists() {
    ensureIconExists();

    const QString userAppsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (userAppsDir.isEmpty()) {
        qWarning() << "GlobalShortcutManager: ApplicationsLocation directory is empty";
        return;
    }

    QDir dir(userAppsDir);
    if (!dir.exists()) {
        dir.mkpath(u"."_s);
    }

    const QString desktopFilePath = dir.filePath(kDesktopFileName);
    const QString appPath = QCoreApplication::applicationFilePath();

    bool needsWrite = true;
    if (QFile::exists(desktopFilePath)) {
        QFile existingFile(desktopFilePath);
        if (existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(existingFile.readAll());
            existingFile.close();
            if (content.contains(u"Exec=" + appPath) && content.contains(u"StartupWMClass=io.github.qtranscribe"_s) &&
                content.contains(u"Icon=io.github.qtranscribe"_s)) {
                qCDebug(lcShortcut) << "Desktop file is up to date:" << desktopFilePath;
                needsWrite = false;
            }
        }
    }

    if (needsWrite) {
        QFile file(desktopFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << u"[Desktop Entry]\n"_s << u"Type=Application\n"_s << u"Name=QTranscribe\n"_s << u"Exec="_s << appPath
                << u"\n"_s << u"Icon=io.github.qtranscribe\n"_s << u"Categories=Utility;\n"_s << u"Terminal=false\n"_s
                << u"StartupWMClass=io.github.qtranscribe\n"_s << u"Comment=Speech to Text Application\n"_s;
            file.close();
            qCDebug(lcShortcut) << "Created/updated desktop file at" << desktopFilePath << "with Exec =" << appPath;
        }
    }

    // Clean up obsolete legacy desktop file if present in user's local applications directory
    const QString legacyDesktopFilePath = dir.filePath(kLegacyDesktopFileName);
    if (QFile::exists(legacyDesktopFilePath)) {
        QFile::remove(legacyDesktopFilePath);
    }
}

void GlobalShortcutManager::registerHostApp() {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return;
    }

    qCDebug(lcShortcut) << "Registering host application ID with XDG Desktop Portal...";

    QDBusMessage msg =
        QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
                                       u"org.freedesktop.host.portal.Registry"_s, u"Register"_s);

    QVariantMap options;
    msg << kApplicationId << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCDebug(lcShortcut) << "org.freedesktop.host.portal.Registry.Register:" << reply.errorMessage();
        if (reply.errorName().contains(u"UnknownInterface"_s) ||
            reply.errorMessage().contains(u"No such interface"_s)) {
            QDBusMessage fallbackMsg = QDBusMessage::createMethodCall(
                u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
                u"org.freedesktop.portal.Registry"_s, u"Register"_s);
            fallbackMsg << kApplicationId << options;
            QDBusMessage fallbackReply = QDBusConnection::sessionBus().call(fallbackMsg);
            if (fallbackReply.type() == QDBusMessage::ErrorMessage) {
                qCDebug(lcShortcut) << "org.freedesktop.portal.Registry.Register fallback:"
                                    << fallbackReply.errorMessage();
            } else {
                qCDebug(lcShortcut) << "Registered host app id with portal registry (fallback) successfully";
            }
        }
    } else {
        qCDebug(lcShortcut) << "Registered host app id" << kApplicationId << "with portal registry successfully";
    }
}

void GlobalShortcutManager::initializePortal() {
    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning() << "GlobalShortcutManager: D-Bus session bus not connected";
        setStatusMessage(u"D-Bus session bus not connected"_s);
        setSupported(false);
        setAvailable(false);
        return;
    }

    if (m_sessionRequested && !m_sessionHandle.path().isEmpty()) {
        bindShortcuts();
        return;
    }

    ensureDesktopFileExists();
    registerHostApp();
    createSession();
}

void GlobalShortcutManager::createSession() {
    m_sessionRequested = true;
    qCDebug(lcShortcut) << "Requesting XDG GlobalShortcuts portal session...";
    setStatusMessage(u"Requesting GlobalShortcuts session..."_s);

    QDBusMessage msg =
        QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
                                       u"org.freedesktop.portal.GlobalShortcuts"_s, u"CreateSession"_s);

    QVariantMap options;
    options.insert(u"handle_token"_s, m_handleToken);
    options.insert(u"session_handle_token"_s, m_sessionHandleToken);

    msg << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "GlobalShortcutManager: CreateSession failed:" << reply.errorMessage();
        const QString errMsg = reply.errorMessage();
        if (errMsg.contains(u"ServiceUnknown"_s) || errMsg.contains(u"UnknownMethod"_s) ||
            errMsg.contains(u"UnknownInterface"_s) || errMsg.contains(u"No such interface"_s) ||
            errMsg.contains(u"not provided by any .service files"_s)) {
            setSupported(false);
            setStatusMessage(u"Global shortcuts not supported on your desktop environment"_s);
        } else {
            setSupported(true);
            setStatusMessage(u"Global shortcuts portal error: %1"_s.arg(errMsg));
        }
        setAvailable(false);
        return;
    }

    if (reply.arguments().isEmpty()) {
        qWarning() << "GlobalShortcutManager: CreateSession response is empty";
        setStatusMessage(u"Global shortcuts portal returned empty response"_s);
        setAvailable(false);
        return;
    }

    QDBusObjectPath requestPath = reply.arguments().first().value<QDBusObjectPath>();
    qCDebug(lcShortcut) << "CreateSession DBus request path:" << requestPath.path();

    QDBusConnection::sessionBus().connect(u"org.freedesktop.portal.Desktop"_s, requestPath.path(),
                                          u"org.freedesktop.portal.Request"_s, u"Response"_s, this,
                                          SLOT(onCreateSessionResponse(uint, QVariantMap)));
}

void GlobalShortcutManager::onCreateSessionResponse(uint responseCode, const QVariantMap& results) {
    if (responseCode != 0) {
        qWarning() << "GlobalShortcutManager: CreateSession response error code:" << responseCode;
        setSupported(true);
        setStatusMessage(u"Global shortcut permission denied in system settings"_s);
        setAvailable(false);
        return;
    }

    if (results.contains(u"session_handle"_s)) {
        QString sessionStr = results.value(u"session_handle"_s).toString();
        m_sessionHandle = QDBusObjectPath(sessionStr);
    } else {
        QString baseService = QDBusConnection::sessionBus().baseService();
        baseService.replace(u'.', u'_');
        if (baseService.startsWith(u':')) {
            baseService.remove(0, 1);
        }
        m_sessionHandle =
            QDBusObjectPath(u"/org/freedesktop/portal/desktop/session/%1/%2"_s.arg(baseService, m_sessionHandleToken));
    }

    qCDebug(lcShortcut) << "Portal session created successfully -> Session handle:" << m_sessionHandle.path();

    QDBusConnection::sessionBus().connect(u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
                                          u"org.freedesktop.portal.GlobalShortcuts"_s, u"Activated"_s, this,
                                          SLOT(onShortcutActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));

    QDBusConnection::sessionBus().connect(
        u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
        u"org.freedesktop.portal.GlobalShortcuts"_s, u"Deactivated"_s, this,
        SLOT(onShortcutDeactivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));

    bindShortcuts();
}

void GlobalShortcutManager::bindShortcuts() {
    qCDebug(lcShortcut) << "Sending BindShortcuts request for Ctrl+Shift+Space...";
    setStatusMessage(u"Binding global shortcut (Ctrl+Shift+Space)..."_s);

    quint32 randVal = QRandomGenerator::global()->generate();
    QString bindReqToken = u"stt_bind_%1"_s.arg(randVal);

    QDBusMessage msg =
        QDBusMessage::createMethodCall(u"org.freedesktop.portal.Desktop"_s, u"/org/freedesktop/portal/desktop"_s,
                                       u"org.freedesktop.portal.GlobalShortcuts"_s, u"BindShortcuts"_s);

    ShortcutList shortcuts;
    PortalShortcut sc;
    sc.id = u"toggle-recording"_s;
    sc.options.insert(u"description"_s, u"Toggle speech-to-text recording"_s);
    sc.options.insert(u"preferred_trigger"_s, u"CTRL+SHIFT+space"_s);
    shortcuts.append(sc);

    QVariantMap options;
    options.insert(u"handle_token"_s, bindReqToken);

    msg << m_sessionHandle << QVariant::fromValue(shortcuts) << QString() << options;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "GlobalShortcutManager: BindShortcuts failed:" << reply.errorMessage();
        setStatusMessage(u"Failed to bind global shortcut: %1"_s.arg(reply.errorMessage()));
        setAvailable(false);
        return;
    }

    if (!reply.arguments().isEmpty()) {
        QDBusObjectPath requestPath = reply.arguments().first().value<QDBusObjectPath>();
        qCDebug(lcShortcut) << "BindShortcuts request path:" << requestPath.path();
        QDBusConnection::sessionBus().connect(u"org.freedesktop.portal.Desktop"_s, requestPath.path(),
                                              u"org.freedesktop.portal.Request"_s, u"Response"_s, this,
                                              SLOT(onBindShortcutsResponse(uint, QVariantMap)));
    }
}

void GlobalShortcutManager::onBindShortcutsResponse(uint responseCode, const QVariantMap& results) {
    Q_UNUSED(results)

    if (responseCode != 0) {
        qWarning() << "GlobalShortcutManager: BindShortcuts response error code:" << responseCode;
        setStatusMessage(u"Global shortcut binding rejected by user/desktop"_s);
        setAvailable(false);
        return;
    }

    qCDebug(lcShortcut) << "Global shortcuts successfully bound to portal!";
    setAvailable(true);
    setStatusMessage(u"Global shortcut active (Ctrl+Shift+Space)"_s);
}

void GlobalShortcutManager::onShortcutActivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId,
                                                qulonglong timestamp, const QVariantMap& options) {
    Q_UNUSED(timestamp)
    Q_UNUSED(options)

    if (sessionHandle == m_sessionHandle) {
        qCDebug(lcShortcut) << "Global shortcut activated signal received -> ID:" << shortcutId;
        emit shortcutActivated(shortcutId);
    }
}

void GlobalShortcutManager::onShortcutDeactivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId,
                                                  qulonglong timestamp, const QVariantMap& options) {
    Q_UNUSED(timestamp)
    Q_UNUSED(options)

    if (sessionHandle == m_sessionHandle) {
        qCDebug(lcShortcut) << "Global shortcut deactivated signal received -> ID:" << shortcutId;
        emit shortcutDeactivated(shortcutId);
    }
}
