#include "WhisperModelManager.h"

#include <QSettings>

using namespace Qt::StringLiterals;

namespace {
const auto kDefaultSelectedModel = u"tiny.en"_s;
} // namespace

WhisperModelManager::WhisperModelManager(QObject* parent)
    : WhisperModelManager(std::make_unique<WhisperModelStorage>(), parent) { }

WhisperModelManager::WhisperModelManager(std::unique_ptr<WhisperModelStorage> storage, QObject* parent)
    : QAbstractListModel(parent)
    , m_storage(std::move(storage)) {
    if (!m_storage) {
        m_storage = std::make_unique<WhisperModelStorage>(this);
    } else {
        m_storage->setParent(this);
    }

    setupStorageConnections();

    QSettings settings;
    m_selectedModelId = settings.value(u"Whisper/SelectedModel"_s, kDefaultSelectedModel).toString();
    if (m_storage->findModelIndex(m_selectedModelId) < 0 && m_storage->modelCount() > 0) {
        m_selectedModelId = m_storage->models().first().id;
    }
}

WhisperModelManager::~WhisperModelManager() = default;

void WhisperModelManager::setupStorageConnections() {
    connect(m_storage.get(), &WhisperModelStorage::modelsChanged, this, [this]() {
        beginResetModel();
        endResetModel();
    });

    connect(m_storage.get(), &WhisperModelStorage::modelStatusChanged, this,
            [this](int idx, const QString& /*modelId*/, bool /*isInstalled*/, qint64 /*installedSizeBytes*/) {
                if (idx >= 0 && idx < m_storage->modelCount()) {
                    const QModelIndex modelIdx = index(idx);
                    emit dataChanged(modelIdx, modelIdx,
                                     {IsInstalledRole, IsSelectedRole, InstalledSizeFormattedRole, CanDeleteRole});
                }
                emit modelStatusChanged();
                emit selectedModelChanged();
            });

    connect(m_storage.get(), &WhisperModelStorage::isDownloadingAnyChanged, this,
            &WhisperModelManager::isDownloadingAnyChanged);

    connect(m_storage.get(), &WhisperModelStorage::downloadProgressChanged, this,
            [this](int idx, const QString& /*modelId*/, qreal /*progress*/, qint64 /*bytesReceived*/,
                   qint64 /*totalBytes*/, const QString& /*speedFormatted*/) {
                if (idx >= 0 && idx < m_storage->modelCount()) {
                    const QModelIndex modelIdx = index(idx);
                    emit dataChanged(
                        modelIdx, modelIdx,
                        {IsDownloadingRole, ProgressRole, BytesReceivedRole, TotalBytesRole, SpeedFormattedRole});
                }
                emit downloadProgressChanged();
            });

    connect(m_storage.get(), &WhisperModelStorage::downloadFinished, this,
            [this](const QString& modelId, bool success, const QString& error) {
                const int idx = m_storage->findModelIndex(modelId);
                if (idx >= 0) {
                    const QModelIndex modelIdx = index(idx);
                    emit dataChanged(modelIdx, modelIdx,
                                     {IsDownloadingRole, IsInstalledRole, IsSelectedRole, ProgressRole,
                                      InstalledSizeFormattedRole, CanDeleteRole});
                }
                if (success && !isSelectedModelInstalled()) {
                    setSelectedModelId(modelId);
                }
                emit downloadProgressChanged();
                emit isDownloadingAnyChanged();
                emit modelStatusChanged();
                emit selectedModelChanged();
                emit modelDownloadFinished(modelId, success, error);
            });

    connect(m_storage.get(), &WhisperModelStorage::diskSpaceChanged, this, &WhisperModelManager::diskSpaceChanged);

    connect(m_storage.get(), &WhisperModelStorage::lastErrorChanged, this, &WhisperModelManager::lastErrorChanged);
}

int WhisperModelManager::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_storage) {
        return 0;
    }
    return m_storage->modelCount();
}

QVariant WhisperModelManager::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_storage || index.row() < 0 || index.row() >= m_storage->modelCount()) {
        return {};
    }

    const auto& item = m_storage->models().at(index.row());
    switch (role) {
        case IdRole:
            return item.id;
        case NameRole:
            return item.name;
        case FileNameRole:
            return item.fileName;
        case DownloadUrlRole:
            return item.downloadUrl;
        case SizeBytesRole:
            return item.sizeBytes;
        case SizeFormattedRole:
            return item.sizeFormatted;
        case MemoryFormattedRole:
            return item.memoryFormatted;
        case DescriptionRole:
            return item.description;
        case IsInstalledRole:
            return item.isInstalled;
        case IsSelectedRole:
            return item.isInstalled && (item.id == m_selectedModelId);
        case IsDownloadingRole:
            return item.isDownloading;
        case ProgressRole:
            return item.progress;
        case BytesReceivedRole:
            return item.bytesReceived;
        case TotalBytesRole:
            return item.totalBytes;
        case SpeedFormattedRole:
            return item.speedFormatted;
        case InstalledSizeFormattedRole:
            return item.installedSizeFormatted;
        case CanDeleteRole:
            return item.isInstalled;
        default:
            return {};
    }
}

QHash<int, QByteArray> WhisperModelManager::roleNames() const {
    return {{IdRole, "modelId"},
            {NameRole, "name"},
            {FileNameRole, "fileName"},
            {DownloadUrlRole, "downloadUrl"},
            {SizeBytesRole, "sizeBytes"},
            {SizeFormattedRole, "sizeFormatted"},
            {MemoryFormattedRole, "memoryFormatted"},
            {DescriptionRole, "description"},
            {IsInstalledRole, "isInstalled"},
            {IsSelectedRole, "isSelected"},
            {IsDownloadingRole, "isDownloading"},
            {ProgressRole, "progress"},
            {BytesReceivedRole, "bytesReceived"},
            {TotalBytesRole, "totalBytes"},
            {SpeedFormattedRole, "speedFormatted"},
            {InstalledSizeFormattedRole, "installedSizeFormatted"},
            {CanDeleteRole, "canDelete"}};
}

WhisperModelStorage* WhisperModelManager::storage() const {
    return m_storage.get();
}

QString WhisperModelManager::modelsDirectory() const {
    return m_storage ? m_storage->modelsDirectory() : QString();
}

QString WhisperModelManager::selectedModelId() const {
    return m_selectedModelId;
}

void WhisperModelManager::setSelectedModelId(const QString& id) {
    if (m_selectedModelId != id) {
        const int oldIdx = m_storage ? m_storage->findModelIndex(m_selectedModelId) : -1;
        m_selectedModelId = id;
        const int newIdx = m_storage ? m_storage->findModelIndex(m_selectedModelId) : -1;

        QSettings settings;
        settings.setValue(u"Whisper/SelectedModel"_s, m_selectedModelId);

        if (oldIdx >= 0) {
            const QModelIndex modelIdx = index(oldIdx);
            emit dataChanged(modelIdx, modelIdx, {IsSelectedRole});
        }
        if (newIdx >= 0) {
            const QModelIndex modelIdx = index(newIdx);
            emit dataChanged(modelIdx, modelIdx, {IsSelectedRole});
        }

        emit selectedModelChanged();
        emit modelStatusChanged();
    }
}

QString WhisperModelManager::selectedModelPath() const {
    return m_storage ? m_storage->getModelPath(m_selectedModelId) : QString();
}

QString WhisperModelManager::selectedModelName() const {
    if (m_storage) {
        const auto item = m_storage->model(m_selectedModelId);
        if (item.has_value()) {
            return item->name;
        }
    }
    return m_selectedModelId;
}

bool WhisperModelManager::isSelectedModelInstalled() const {
    return m_storage ? m_storage->isModelInstalled(m_selectedModelId) : false;
}

bool WhisperModelManager::isDownloadingAny() const {
    return m_storage ? m_storage->isDownloadingAny() : false;
}

QString WhisperModelManager::downloadingModelId() const {
    return m_storage ? m_storage->downloadingModelId() : QString();
}

qreal WhisperModelManager::downloadProgress() const {
    return m_storage ? m_storage->downloadProgress() : 0.0;
}

QString WhisperModelManager::downloadSpeedFormatted() const {
    return m_storage ? m_storage->downloadSpeedFormatted() : QString();
}

QString WhisperModelManager::downloadBytesFormatted() const {
    return m_storage ? m_storage->downloadBytesFormatted() : QString();
}

QString WhisperModelManager::lastError() const {
    return m_storage ? m_storage->lastError() : QString();
}

QString WhisperModelManager::availableDiskSpaceFormatted() const {
    return m_storage ? m_storage->availableDiskSpaceFormatted() : QString();
}

bool WhisperModelManager::isModelInstalled(const QString& modelId) const {
    return m_storage ? m_storage->isModelInstalled(modelId) : false;
}

QString WhisperModelManager::getModelPath(const QString& modelId) const {
    return m_storage ? m_storage->getModelPath(modelId) : QString();
}

void WhisperModelManager::startDownload(const QString& modelId) {
    if (m_storage) {
        m_storage->startDownload(modelId);
    }
}

void WhisperModelManager::cancelDownload(const QString& modelId) {
    if (m_storage) {
        m_storage->cancelDownload(modelId);
    }
}

bool WhisperModelManager::deleteModel(const QString& modelId) {
    if (m_storage) {
        return m_storage->deleteModel(modelId);
    }
    return false;
}

void WhisperModelManager::refreshModelList() {
    if (m_storage) {
        m_storage->refreshModelList();
    }
}

void WhisperModelManager::checkDiskSpace() {
    if (m_storage) {
        m_storage->checkDiskSpace();
    }
}
