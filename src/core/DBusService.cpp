#include "DBusService.h"

#include "LoggingCategories.h"
#include "SpeechController.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>

using namespace Qt::StringLiterals;

const QString DBusService::kDbusServiceName = u"io.github.qtranscribe.SpeechService"_s;
const QString DBusService::kDbusObjectPath = u"/io/github/qtranscribe/SpeechService"_s;
const QString DBusService::kDbusInterfaceName = u"io.github.qtranscribe.SpeechService"_s;

DBusService::DBusService(QObject* parent)
    : QObject(parent) { }

bool DBusService::sendRemoteCommand(const QString& method) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        return false;
    }

    QDBusInterface remoteService(kDbusServiceName, kDbusObjectPath, kDbusInterfaceName, QDBusConnection::sessionBus());
    if (remoteService.isValid()) {
        QDBusMessage reply = remoteService.call(method);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "DBusService: Remote command" << method << "failed:" << reply.errorMessage();
            return false;
        }
        return true;
    }
    return false;
}

bool DBusService::registerService() {
    if (!QDBusConnection::sessionBus().isConnected()) {
        qWarning() << "DBusService: Session bus not connected";
        return false;
    }

    if (!QDBusConnection::sessionBus().registerService(kDbusServiceName)) {
        qWarning() << "DBusService: Failed to register service" << kDbusServiceName;
        return false;
    }

    return true;
}

bool DBusService::registerController(SpeechController* controller) {
    if (!controller || !QDBusConnection::sessionBus().isConnected()) {
        return false;
    }

    if (!registerService()) {
        return false;
    }

    bool ok = QDBusConnection::sessionBus().registerObject(
        kDbusObjectPath, kDbusInterfaceName, controller,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllInvokables);

    if (ok) {
        m_registered = true;
        qCDebug(lcSpeech) << "DBusService: SpeechController exported to D-Bus at" << kDbusObjectPath;
    } else {
        qWarning() << "DBusService: Failed to register object on D-Bus";
    }

    return ok;
}

bool DBusService::isRegistered() const {
    return m_registered;
}
