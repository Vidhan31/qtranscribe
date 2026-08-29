#pragma once

#include "ModelDownloader.h"
#include "WhisperModelCatalog.h"

#include <QList>
#include <QObject>
#include <QString>

#include <memory>
#include <optional>

class WhisperModelStorage : public QObject {
    Q_OBJECT

public:
    explicit WhisperModelStorage(QObject* parent = nullptr);
    explicit WhisperModelStorage(std::unique_ptr<ModelDownloader> downloader, QObject* parent = nullptr);
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

private:
    void setupDownloaderConnections();
    void setLastError(const QString& error);

    WhisperModelCatalog m_catalog;
    std::unique_ptr<ModelDownloader> m_downloader;
    QString m_customModelsDirectory;
    QList<WhisperModelItem> m_models;
    QString m_lastError;
    qint64 m_availableDiskSpace = 0;
};
