#pragma once

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QObject>
#include <QQmlEngine>
#include <QString>

using namespace Qt::StringLiterals;

class LicenseHelper : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString licenseText READ licenseText CONSTANT FINAL)

public:
    explicit LicenseHelper(QObject* parent = nullptr)
        : QObject(parent) { }

    QString licenseText() const {
        QFile file(u":/qt/qml/QTranscribe/license/LICENSE"_s);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(file.readAll());
        }
        return u"GNU GENERAL PUBLIC LICENSE\n"
               "Version 3, 29 June 2007\n\n"
               "Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>\n"
               "Everyone is permitted to copy and distribute verbatim copies\n"
               "of this license document, but changing it is not allowed.\n"_s;
    }

    Q_INVOKABLE void copyToClipboard(const QString& text) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        if (clipboard) {
            clipboard->setText(text);
        }
    }
};
