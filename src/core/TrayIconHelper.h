#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class TrayIconHelper : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString trayIconDarkPath READ trayIconDarkPath CONSTANT FINAL)
    Q_PROPERTY(QString trayIconLightPath READ trayIconLightPath CONSTANT FINAL)
    Q_PROPERTY(QString trayIconRecordingDarkPath READ trayIconRecordingDarkPath CONSTANT FINAL)
    Q_PROPERTY(QString trayIconRecordingLightPath READ trayIconRecordingLightPath CONSTANT FINAL)
    Q_PROPERTY(QString trayIconName READ trayIconName CONSTANT FINAL)
    Q_PROPERTY(QString trayIconRecordingName READ trayIconRecordingName CONSTANT FINAL)

public:
    explicit TrayIconHelper(QObject* parent = nullptr)
        : QObject(parent) { }

    Q_INVOKABLE QString trayIconPath(bool isDark) const { return isDark ? trayIconDarkPath() : trayIconLightPath(); }

    Q_INVOKABLE QString trayIconRecordingPath(bool isDark) const {
        return isDark ? trayIconRecordingDarkPath() : trayIconRecordingLightPath();
    }

    QString trayIconDarkPath() const { return QStringLiteral("qrc:/qt/qml/QTranscribe/assets/mute-dark.svg"); }
    QString trayIconLightPath() const { return QStringLiteral("qrc:/qt/qml/QTranscribe/assets/mute.svg"); }
    QString trayIconRecordingDarkPath() const {
        return QStringLiteral("qrc:/qt/qml/QTranscribe/assets/microphone-dark.svg");
    }
    QString trayIconRecordingLightPath() const {
        return QStringLiteral("qrc:/qt/qml/QTranscribe/assets/microphone.svg");
    }

    QString trayIconName() const { return QStringLiteral("qtranscribe-tray"); }
    QString trayIconRecordingName() const { return QStringLiteral("qtranscribe-tray-recording"); }

signals:
    void iconsReady();
};
