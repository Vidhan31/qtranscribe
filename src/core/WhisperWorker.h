#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>

struct whisper_context;

class WhisperWorker : public QObject {
    Q_OBJECT

public:
    explicit WhisperWorker(QObject* parent = nullptr);
    ~WhisperWorker() override;

public slots:
    void loadModel(const QString& modelPath, bool useGpu = true);
    void unloadModel();
    void transcribe(const QByteArray& wavData, const QString& language = QString(), const QString& prompt = QString());
    void cancel();

signals:
    void modelLoaded(bool success, const QString& error, const QString& activeDevice);
    void modelUnloaded();
    void transcriptionFinished(const QString& text);
    void transcriptionFailed(const QString& error);

private:
    whisper_context* m_ctx = nullptr;
    std::atomic<bool> m_cancelled {false};
    QString m_activeDevice;
};
