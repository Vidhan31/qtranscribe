#include <QCoreApplication>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "keyinjectord/device_interface.h"
#include "keyinjectord/ipc_server.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace std::chrono_literals;

class MockDevice : public keyinjectord::IDevice {
public:
    bool sendCtrlV() override {
        ++ctrlVCalledCount;
        return true;
    }

    std::atomic<int> ctrlVCalledCount{0};
};

class TestIpcServer : public QObject {
    Q_OBJECT

private slots:
    void testValidPasteCommand() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        std::string sockPath = (tempDir.path() + "/test_ipc.sock").toStdString();

        MockDevice mockDevice;
        keyinjectord::IpcServer server(sockPath, mockDevice);

        std::thread serverThread([&server]() { server.run(); });

        // Connect client
        QLocalSocket client;
        client.connectToServer(QString::fromStdString(sockPath));
        QVERIFY(client.waitForConnected(2000));

        // Send valid command
        const QByteArray cmd = "{\"cmd\": \"paste\"}\n";
        client.write(cmd);
        client.flush();

        QVERIFY(client.waitForReadyRead(2000));
        QByteArray response = client.readAll();
        QCOMPARE(response, QByteArray("{\"status\": \"ok\"}\n"));

        QCOMPARE(mockDevice.ctrlVCalledCount.load(), 1);

        client.disconnectFromServer();
        server.stop();
        serverThread.join();
    }

    void testBufferOverflowDisconnect() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        std::string sockPath = (tempDir.path() + "/test_ipc_overflow.sock").toStdString();

        MockDevice mockDevice;
        keyinjectord::IpcServer server(sockPath, mockDevice);

        std::thread serverThread([&server]() { server.run(); });

        QLocalSocket client;
        client.connectToServer(QString::fromStdString(sockPath));
        QVERIFY(client.waitForConnected(2000));

        // Send payload exceeding 1024 bytes without newline delimiter
        QByteArray spamData(1025, 'X');
        client.write(spamData);
        client.flush();

        // Server should detect overflow and immediately close the socket
        QVERIFY(client.waitForDisconnected(2000) || client.state() == QLocalSocket::UnconnectedState);

        server.stop();
        serverThread.join();
    }

    void testMaxClientsLimit() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        std::string sockPath = (tempDir.path() + "/test_ipc_max_clients.sock").toStdString();

        MockDevice mockDevice;
        keyinjectord::IpcServer server(sockPath, mockDevice);

        std::thread serverThread([&server]() { server.run(); });

        std::vector<std::unique_ptr<QLocalSocket>> clients;
        // Connect up to kMaxClients (8 clients)
        for (size_t i = 0; i < keyinjectord::kMaxClients; ++i) {
            auto client = std::make_unique<QLocalSocket>();
            client->connectToServer(QString::fromStdString(sockPath));
            QVERIFY(client->waitForConnected(2000));
            clients.push_back(std::move(client));
        }

        // 9th client connection attempt should be rejected / disconnected by server
        QLocalSocket rejectedClient;
        rejectedClient.connectToServer(QString::fromStdString(sockPath));
        if (rejectedClient.state() == QLocalSocket::ConnectedState || rejectedClient.waitForConnected(1000)) {
            // Once connected at OS level, server should close it immediately upon accept
            QVERIFY(rejectedClient.waitForDisconnected(2000) ||
                    rejectedClient.state() == QLocalSocket::UnconnectedState);
        }

        // Clean up
        for (auto& client : clients) {
            client->disconnectFromServer();
        }

        server.stop();
        serverThread.join();
    }

    void testUnknownCommandGracefulHandling() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        std::string sockPath = (tempDir.path() + "/test_ipc_unknown.sock").toStdString();

        MockDevice mockDevice;
        keyinjectord::IpcServer server(sockPath, mockDevice);

        std::thread serverThread([&server]() { server.run(); });

        QLocalSocket client;
        client.connectToServer(QString::fromStdString(sockPath));
        QVERIFY(client.waitForConnected(2000));

        const QByteArray cmd = "{\"cmd\": \"nonexistent_command\"}\n";
        client.write(cmd);
        client.flush();

        QVERIFY(client.waitForReadyRead(2000));
        QByteArray response = client.readAll();
        QCOMPARE(response, QByteArray("{\"status\": \"ok\"}\n"));

        // Paste action should NOT have been invoked
        QCOMPARE(mockDevice.ctrlVCalledCount.load(), 0);

        client.disconnectFromServer();
        server.stop();
        serverThread.join();
    }
};

QTEST_MAIN(TestIpcServer)
#include "test_ipc_server.moc"
