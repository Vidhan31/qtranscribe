#pragma once

#include "AbstractSttClient.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QThread>

class WhisperWorker;

class WhisperSttClient : public AbstractSttClient {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool isModelInstalled READ isModelInstalled NOTIFY modelStatusChanged FINAL)
    Q_PROPERTY(bool isModelLoaded READ isModelLoaded NOTIFY modelStatusChanged FINAL)
    Q_PROPERTY(QString modelPath READ modelPath NOTIFY modelStatusChanged FINAL)
    Q_PROPERTY(QString modelsDirectory READ modelsDirectory CONSTANT FINAL)
    Q_PROPERTY(QString modelFileName READ modelFileName CONSTANT FINAL)
    Q_PROPERTY(QString downloadUrl READ downloadUrl CONSTANT FINAL)
    Q_PROPERTY(QString computeDevice READ computeDevice NOTIFY computeDeviceChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(bool isVulkanSupported READ isVulkanSupported CONSTANT FINAL)

public:
    explicit WhisperSttClient(QObject* parent = nullptr);
    ~WhisperSttClient() override;

    bool isModelInstalled() const;
    bool isModelLoaded() const;
    QString modelPath() const;
    QString modelsDirectory() const;
    QString modelFileName() const;
    QString downloadUrl() const;
    QString computeDevice() const;
    QString lastError() const;
    bool isVulkanSupported() const;

    bool isReady() const override;
    bool isBusy() const override;

    Q_INVOKABLE void transcribe(const QByteArray& wavData) override;
    Q_INVOKABLE void cancel() override;
    Q_INVOKABLE void loadModel();
    Q_INVOKABLE void unloadModel();
    Q_INVOKABLE void checkModelStatus();

signals:
    void modelStatusChanged();
    void computeDeviceChanged();
    void lastErrorChanged();

    // Internal signals routed to worker
    void requestLoadModel(const QString& path, bool useGpu);
    void requestUnloadModel();
    void requestTranscribe(const QByteArray& wavData);
    void requestCancel();

private slots:
    void onWorkerModelLoaded(bool success, const QString& error, const QString& activeDevice);
    void onWorkerModelUnloaded();
    void onWorkerTranscriptionFinished(const QString& text);
    void onWorkerTranscriptionFailed(const QString& error);

private:
    void setLastError(const QString& error);
    void setBusy(bool busy);
    QString resolveModelPath() const;

    QThread m_workerThread;
    WhisperWorker* m_worker = nullptr;

    bool m_modelLoaded = false;
    bool m_busy = false;
    QString m_computeDevice;
    QString m_lastError;

    static constexpr const char* kModelFileName = "ggml-tiny.en.bin";
    static constexpr const char* kDownloadUrl =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin";
};
