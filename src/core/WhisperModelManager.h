#pragma once

#include "WhisperModelStorage.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

#include <memory>

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
        MemoryFormattedRole,
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
    explicit WhisperModelManager(std::unique_ptr<WhisperModelStorage> storage, QObject* parent = nullptr);
    ~WhisperModelManager() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    WhisperModelStorage* storage() const;

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

private:
    void setupStorageConnections();

    std::unique_ptr<WhisperModelStorage> m_storage;
    QString m_selectedModelId;
};
