#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>
#include <optional>

class QFile;
class QNetworkReply;

struct WhisperModelItem {
    QString id;
    QString name;
    QString fileName;
    QString downloadUrl;
    qint64 sizeBytes = 0;
    QString sizeFormatted;
    QString description;
    bool isInstalled = false;
    qint64 installedSizeBytes = 0;
    QString installedSizeFormatted;

    bool isDownloading = false;
    qreal progress = 0.0;
    qint64 bytesReceived = 0;
    qint64 totalBytes = 0;
    QString speedFormatted;

    WhisperModelItem() = default;
    WhisperModelItem(QString id_, QString name_, QString fileName_, QString downloadUrl_, qint64 sizeBytes_,
                     QString sizeFormatted_, QString description_)
        : id(std::move(id_))
        , name(std::move(name_))
        , fileName(std::move(fileName_))
        , downloadUrl(std::move(downloadUrl_))
        , sizeBytes(sizeBytes_)
        , sizeFormatted(std::move(sizeFormatted_))
        , description(std::move(description_)) { }
};

class WhisperModelStorage : public QObject {
    Q_OBJECT

public:
    explicit WhisperModelStorage(QObject* parent = nullptr);
    ~WhisperModelStorage() override;

    QString modelsDirectory() const;
    void setModelsDirectory(const QString& path);

    const QList<WhisperModelItem>& models() const;
    std::optional<WhisperModelItem> model(const QString& modelId) const;
    int modelCount() const;
    int findModelIndex(const QString& modelId) const;

    bool isModelInstalled(const QString& modelId) const;
    QString getModelPath(const QString& modelId) const;

    qint64 availableDiskSpace() const;
    QString availableDiskSpaceFormatted() const;

    bool isDownloadingAny() const;
    QString downloadingModelId() const;
    qreal downloadProgress() const;
    QString downloadSpeedFormatted() const;
    QString downloadBytesFormatted() const;
    QString lastError() const;

    static QString formatBytes(qint64 bytes);

public slots:
    void refreshModelList();
    void scanInstalledModels();
    void checkDiskSpace();
    void cleanupOrphanedPartFiles();
    bool startDownload(const QString& modelId);
    void cancelDownload(const QString& modelId = QString());
    bool deleteModel(const QString& modelId);

signals:
    void modelsChanged();
    void modelStatusChanged(int index, const QString& modelId, bool isInstalled, qint64 installedSizeBytes);
    void isDownloadingAnyChanged();
    void downloadProgressChanged(int index, const QString& modelId, qreal progress, qint64 bytesReceived,
                                 qint64 totalBytes, const QString& speedFormatted);
    void downloadFinished(const QString& modelId, bool success, const QString& error);
    void diskSpaceChanged(qint64 availableBytes);
    void lastErrorChanged(const QString& error);

private slots:
    void onDownloadReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void initPresets();
    void setLastError(const QString& error);

    QString m_customModelsDirectory;
    QList<WhisperModelItem> m_models;
    QString m_lastError;
    qint64 m_availableDiskSpace = 0;

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_currentReply;
    std::unique_ptr<QFile> m_partFile;
    QString m_downloadingModelId;
    QString m_downloadAbortReason;
    qreal m_currentProgress = 0.0;
    qint64 m_currentBytesReceived = 0;
    qint64 m_currentTotalBytes = 0;
    QString m_currentSpeedFormatted;
    QElapsedTimer m_downloadTimer;
    qint64 m_lastSpeedBytes = 0;
    qint64 m_lastSpeedTimeMs = 0;
};
