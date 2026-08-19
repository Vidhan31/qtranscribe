#pragma once

#include <QObject>
#include <QString>

class SpeechController;

class DBusService : public QObject {
    Q_OBJECT

public:
    explicit DBusService(QObject* parent = nullptr);
    ~DBusService() override = default;

    bool registerService();
    bool registerController(SpeechController* controller);
    bool isRegistered() const;

    static bool sendRemoteCommand(const QString& method);

    static const QString kDbusServiceName;
    static const QString kDbusObjectPath;
    static const QString kDbusInterfaceName;

private:
    bool m_registered = false;
};
