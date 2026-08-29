#pragma once

#include <QCoreApplication>
#include <QList>
#include <QString>

#include <optional>

struct WhisperModelItem {
    QString id = {};
    QString name = {};
    QString fileName = {};
    QString downloadUrl = {};
    qint64 sizeBytes = 0;
    QString sizeFormatted = {};
    QString memoryFormatted = {};
    QString description = {};
    bool isInstalled = false;
    qint64 installedSizeBytes = 0;
    QString installedSizeFormatted = {};

    bool isDownloading = false;
    qreal progress = 0.0;
    qint64 bytesReceived = 0;
    qint64 totalBytes = 0;
    QString speedFormatted = {};
};

class WhisperModelCatalog {
    Q_DECLARE_TR_FUNCTIONS(WhisperModelCatalog)

public:
    WhisperModelCatalog();

    const QList<WhisperModelItem>& presets() const;
    std::optional<WhisperModelItem> model(const QString& modelId) const;
    int modelCount() const;
    int findModelIndex(const QString& modelId) const;

private:
    void initPresets();

    QList<WhisperModelItem> m_presets;
};
