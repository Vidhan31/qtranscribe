#include "ModelDownloader.h"
#include "WhisperModelCatalog.h"
#include "WhisperModelStorage.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class WhisperStorageTest : public QObject {
    Q_OBJECT

private slots:
    void testCatalogStandalone() {
        WhisperModelCatalog catalog;
        QCOMPARE(catalog.modelCount(), 10);
        QCOMPARE(catalog.presets().size(), 10);

        for (const auto& item : catalog.presets()) {
            QVERIFY(!item.id.isEmpty());
            QVERIFY(!item.name.isEmpty());
            QVERIFY(!item.fileName.isEmpty());
            QVERIFY(item.fileName.endsWith(u".bin"_s));
            QVERIFY(item.downloadUrl.startsWith(u"https://huggingface.co/"_s));
            QVERIFY(item.sizeBytes > 0);
            QVERIFY(!item.sizeFormatted.isEmpty());
            QVERIFY(!item.memoryFormatted.isEmpty());
            QVERIFY(!item.description.isEmpty());
        }

        const int tinyIdx = catalog.findModelIndex(u"tiny.en"_s);
        QVERIFY(tinyIdx >= 0);

        const auto modelOpt = catalog.model(u"tiny.en"_s);
        QVERIFY(modelOpt.has_value());
        QCOMPARE(modelOpt->id, u"tiny.en"_s);
        QCOMPARE(modelOpt->fileName, u"ggml-tiny.en.bin"_s);

        const auto invalidOpt = catalog.model(u"non_existent_model"_s);
        QVERIFY(!invalidOpt.has_value());
        QCOMPARE(catalog.findModelIndex(u"non_existent_model"_s), -1);
    }

    void testModelDownloaderBasics() {
        ModelDownloader downloader;
        QVERIFY(!downloader.isDownloadingAny());
        QVERIFY(downloader.downloadingModelId().isEmpty());
        QCOMPARE(downloader.downloadProgress(), 0.0);
        QCOMPARE(downloader.downloadBytesReceived(), 0);
        QCOMPARE(downloader.downloadTotalBytes(), 0);
        QVERIFY(downloader.downloadSpeedFormatted().isEmpty());
        QVERIFY(downloader.downloadBytesFormatted().isEmpty());

        downloader.cancelDownload(u"non_existent"_s);
        QVERIFY(!downloader.isDownloadingAny());

        QCOMPARE(ModelDownloader::formatBytes(0), u"0 B"_s);
        QCOMPARE(ModelDownloader::formatBytes(512), u"512 B"_s);
        QCOMPARE(ModelDownloader::formatBytes(1024), u"1.0 KiB"_s);
        QCOMPARE(ModelDownloader::formatBytes(1024 * 1024), u"1.0 MiB"_s);
        QCOMPARE(ModelDownloader::formatBytes(1024LL * 1024LL * 1024LL), u"1.00 GiB"_s);
    }

    void testPresetsCountAndMetadata() {
        WhisperModelStorage storage;
        QCOMPARE(storage.modelCount(), 10);

        const auto& models = storage.models();
        for (const auto& item : models) {
            QVERIFY(!item.id.isEmpty());
            QVERIFY(!item.name.isEmpty());
            QVERIFY(!item.fileName.isEmpty());
            QVERIFY(item.fileName.endsWith(u".bin"_s));
            QVERIFY(item.downloadUrl.startsWith(u"https://huggingface.co/"_s));
            QVERIFY(item.sizeBytes > 0);
            QVERIFY(!item.sizeFormatted.isEmpty());
            QVERIFY(!item.memoryFormatted.isEmpty());
            QVERIFY(!item.description.isEmpty());
        }

        const int tinyIdx = storage.findModelIndex(u"tiny.en"_s);
        QVERIFY(tinyIdx >= 0);

        const auto modelOpt = storage.model(u"tiny.en"_s);
        QVERIFY(modelOpt.has_value());
        QCOMPARE(modelOpt->id, u"tiny.en"_s);
        QCOMPARE(modelOpt->fileName, u"ggml-tiny.en.bin"_s);
    }

    void testCustomDirectoryAndScanning() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        WhisperModelStorage storage;
        storage.setModelsDirectory(tempDir.path());
        QCOMPARE(storage.modelsDirectory(), tempDir.path());

        QVERIFY(!storage.isModelInstalled(u"tiny.en"_s));

        const QString filePath = tempDir.path() + u"/ggml-tiny.en.bin"_s;
        {
            QFile file(filePath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            constexpr quint32 kGgmlMagic = 0x67676d6c;
            file.write(reinterpret_cast<const char*>(&kGgmlMagic), sizeof(kGgmlMagic));
            file.write("dummy_content_bytes");
            file.close();
        }

        QSignalSpy statusSpy(&storage, &WhisperModelStorage::modelStatusChanged);
        storage.scanInstalledModels();

        QVERIFY(storage.isModelInstalled(u"tiny.en"_s));
        QCOMPARE(storage.getModelPath(u"tiny.en"_s), filePath);
        QCOMPARE(statusSpy.count(), 1);

        const QList<QVariant> args = statusSpy.takeFirst();
        QCOMPARE(args.at(1).toString(), u"tiny.en"_s);
        QCOMPARE(args.at(2).toBool(), true);
    }

    void testModelDeletion() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        WhisperModelStorage storage;
        storage.setModelsDirectory(tempDir.path());

        const QString filePath = tempDir.path() + u"/ggml-tiny.en.bin"_s;
        {
            QFile file(filePath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("dummy");
            file.close();
        }

        storage.scanInstalledModels();
        QVERIFY(storage.isModelInstalled(u"tiny.en"_s));

        QSignalSpy statusSpy(&storage, &WhisperModelStorage::modelStatusChanged);
        const bool deleted = storage.deleteModel(u"tiny.en"_s);

        QVERIFY(deleted);
        QVERIFY(!QFile::exists(filePath));
        QVERIFY(!storage.isModelInstalled(u"tiny.en"_s));
        QCOMPARE(statusSpy.count(), 1);

        const QList<QVariant> args = statusSpy.takeFirst();
        QCOMPARE(args.at(1).toString(), u"tiny.en"_s);
        QCOMPARE(args.at(2).toBool(), false);
    }

    void testOrphanPartFileCleanup() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString partFile = tempDir.path() + u"/ggml-base.en.bin.part"_s;
        {
            QFile file(partFile);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("incomplete_chunk");
            file.close();
        }
        QVERIFY(QFile::exists(partFile));

        WhisperModelStorage storage;
        storage.setModelsDirectory(tempDir.path());

        QVERIFY(!QFile::exists(partFile));
    }

    void testFormatBytes() {
        QCOMPARE(WhisperModelStorage::formatBytes(0), u"0 B"_s);
        QCOMPARE(WhisperModelStorage::formatBytes(512), u"512 B"_s);
        QCOMPARE(WhisperModelStorage::formatBytes(1024), u"1.0 KiB"_s);
        QCOMPARE(WhisperModelStorage::formatBytes(1024 * 1024), u"1.0 MiB"_s);
        QCOMPARE(WhisperModelStorage::formatBytes(1024LL * 1024LL * 1024LL), u"1.00 GiB"_s);
    }
};

QTEST_GUILESS_MAIN(WhisperStorageTest)
#include "test_whisper_storage.moc"
