#pragma once

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantMap>

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
    virtual void retryLast();
    virtual bool isReady() const = 0;
    virtual bool isBusy() const = 0;

    virtual void activate();
    virtual void deactivate();

    virtual QString lastError() const;

signals:
    void transcriptionReady(const QString& text);
    void errorOccurred(const QString& error);
    void busyChanged();
    void readyChanged();
};
