#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

using namespace Qt::StringLiterals;

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

    QString trayIconDarkPath() const { return u"qrc:/qt/qml/QTranscribe/assets/mute-dark.svg"_s; }
    QString trayIconLightPath() const { return u"qrc:/qt/qml/QTranscribe/assets/mute.svg"_s; }
    QString trayIconRecordingDarkPath() const { return u"qrc:/qt/qml/QTranscribe/assets/microphone-dark.svg"_s; }
    QString trayIconRecordingLightPath() const { return u"qrc:/qt/qml/QTranscribe/assets/microphone.svg"_s; }

    QString trayIconName() const { return u"qtranscribe-tray"_s; }
    QString trayIconRecordingName() const { return u"qtranscribe-tray-recording"_s; }

signals:
    void iconsReady();
};
