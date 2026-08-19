#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class TrayIconHelper : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString trayIconPath READ trayIconPath NOTIFY iconsReady FINAL)
    Q_PROPERTY(QString trayIconRecordingPath READ trayIconRecordingPath NOTIFY iconsReady FINAL)
    Q_PROPERTY(QString trayIconName READ trayIconName CONSTANT FINAL)
    Q_PROPERTY(QString trayIconRecordingName READ trayIconRecordingName CONSTANT FINAL)

public:
    explicit TrayIconHelper(QObject* parent = nullptr)
        : QObject(parent) { }

    QString trayIconPath() const { return QStringLiteral("qrc:/qt/qml/QTranscribe/assets/mute.svg"); }
    QString trayIconRecordingPath() const { return QStringLiteral("qrc:/qt/qml/QTranscribe/assets/microphone.svg"); }
    QString trayIconName() const { return QStringLiteral("qtranscribe-tray"); }
    QString trayIconRecordingName() const { return QStringLiteral("qtranscribe-tray-recording"); }

signals:
    void iconsReady();
};
