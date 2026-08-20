#pragma once

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QString>

class AbstractSttClient : public QObject {
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged FINAL)

public:
    explicit AbstractSttClient(QObject* parent = nullptr);
    ~AbstractSttClient() override = default;

    virtual void transcribe(const QByteArray& wavData) = 0;
    virtual void cancel() = 0;
    virtual bool isReady() const = 0;
    virtual bool isBusy() const = 0;

signals:
    void transcriptionReady(const QString& text);
    void errorOccurred(const QString& error);
    void busyChanged();
    void readyChanged();
};
