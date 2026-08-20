#pragma once

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QList>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QString>

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

    // Live download state
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

class WhisperModelManager : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString modelsDirectory READ modelsDirectory CONSTANT FINAL)
    Q_PROPERTY(QString selectedModelId READ selectedModelId WRITE setSelectedModelId NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(QString selectedModelPath READ selectedModelPath NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(QString selectedModelName READ selectedModelName NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(bool isSelectedModelInstalled READ isSelectedModelInstalled NOTIFY selectedModelChanged FINAL)
    Q_PROPERTY(bool isDownloadingAny READ isDownloadingAny NOTIFY isDownloadingAnyChanged FINAL)
    Q_PROPERTY(QString downloadingModelId READ downloadingModelId NOTIFY isDownloadingAnyChanged FINAL)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged FINAL)
    Q_PROPERTY(QString downloadSpeedFormatted READ downloadSpeedFormatted NOTIFY downloadProgressChanged FINAL)
    Q_PROPERTY(QString downloadBytesFormatted READ downloadBytesFormatted NOTIFY downloadProgressChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QString availableDiskSpaceFormatted READ availableDiskSpaceFormatted NOTIFY diskSpaceChanged FINAL)

public:
    enum Roles : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        FileNameRole,
        DownloadUrlRole,
        SizeBytesRole,
        SizeFormattedRole,
        DescriptionRole,
        IsInstalledRole,
        IsSelectedRole,
        IsDownloadingRole,
        ProgressRole,
        BytesReceivedRole,
        TotalBytesRole,
        SpeedFormattedRole,
        InstalledSizeFormattedRole,
        CanDeleteRole
    };
    Q_ENUM(Roles)

    explicit WhisperModelManager(QObject* parent = nullptr);
    ~WhisperModelManager() override;

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString modelsDirectory() const;
    QString selectedModelId() const;
    QString selectedModelPath() const;
    QString selectedModelName() const;
    bool isSelectedModelInstalled() const;

    bool isDownloadingAny() const;
    QString downloadingModelId() const;
    qreal downloadProgress() const;
    QString downloadSpeedFormatted() const;
    QString downloadBytesFormatted() const;
    QString lastError() const;
    QString availableDiskSpaceFormatted() const;

    bool isModelInstalled(const QString& modelId) const;
    QString getModelPath(const QString& modelId) const;

    Q_INVOKABLE void setSelectedModelId(const QString& id);
    Q_INVOKABLE void startDownload(const QString& modelId);
    Q_INVOKABLE void cancelDownload(const QString& modelId = QString());
    Q_INVOKABLE bool deleteModel(const QString& modelId);
    Q_INVOKABLE void refreshModelList();
    Q_INVOKABLE void checkDiskSpace();

signals:
    void selectedModelChanged();
    void modelStatusChanged();
    void isDownloadingAnyChanged();
    void downloadProgressChanged();
    void lastErrorChanged();
    void diskSpaceChanged();
    void modelDownloadFinished(const QString& modelId, bool success, const QString& error);

private slots:
    void onDownloadReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void initPresets();
    void scanInstalledModels();
    void cleanupOrphanedPartFiles();
    void setLastError(const QString& error);
    int findModelIndex(const QString& modelId) const;
    static QString formatBytes(qint64 bytes);

    QList<WhisperModelItem> m_models;
    QString m_selectedModelId;
    QString m_lastError;
    qint64 m_availableDiskSpace = 0;

    // Download state
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_currentReply;
    QScopedPointer<QFile> m_partFile;
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
