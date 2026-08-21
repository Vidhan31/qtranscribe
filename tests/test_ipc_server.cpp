#include "keyinjectord/device_interface.h"
#include "keyinjectord/ipc_server.h"
#include "keyinjectord/protocol.h"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <chrono>
#include <memory>
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

    std::atomic<int> ctrlVCalledCount {0};
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

        QLocalSocket client;
        client.connectToServer(QString::fromStdString(sockPath));
        QVERIFY(client.waitForConnected(2000));

        // Send binary Paste opcode (0x01)
        const char cmdByte = static_cast<char>(keyinjectord::Opcode::Paste);
        client.write(&cmdByte, 1);
        client.flush();

        QVERIFY(client.waitForReadyRead(2000));
        QByteArray response = client.readAll();
        QCOMPARE(response.size(), 1);
        QCOMPARE(static_cast<uint8_t>(response[0]), static_cast<uint8_t>(keyinjectord::ResponseStatus::Ok));

        QCOMPARE(mockDevice.ctrlVCalledCount.load(), 1);

        client.disconnectFromServer();
        server.stop();
        serverThread.join();
    }

    void testPingCommand() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        std::string sockPath = (tempDir.path() + "/test_ipc_ping.sock").toStdString();

        MockDevice mockDevice;
        keyinjectord::IpcServer server(sockPath, mockDevice);

        std::thread serverThread([&server]() { server.run(); });

        QLocalSocket client;
        client.connectToServer(QString::fromStdString(sockPath));
        QVERIFY(client.waitForConnected(2000));

        // Send binary Ping opcode (0x02)
        const char cmdByte = static_cast<char>(keyinjectord::Opcode::Ping);
        client.write(&cmdByte, 1);
        client.flush();

        QVERIFY(client.waitForReadyRead(2000));
        QByteArray response = client.readAll();
        QCOMPARE(response.size(), 1);
        QCOMPARE(static_cast<uint8_t>(response[0]), static_cast<uint8_t>(keyinjectord::ResponseStatus::Ok));

        // No paste action performed on Ping
        QCOMPARE(mockDevice.ctrlVCalledCount.load(), 0);

        client.disconnectFromServer();
        server.stop();
        serverThread.join();
    }

    void testUnknownOpcodeDisconnect() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        std::string sockPath = (tempDir.path() + "/test_ipc_unknown.sock").toStdString();

        MockDevice mockDevice;
        keyinjectord::IpcServer server(sockPath, mockDevice);

        std::thread serverThread([&server]() { server.run(); });

        QLocalSocket client;
        client.connectToServer(QString::fromStdString(sockPath));
        QVERIFY(client.waitForConnected(2000));

        // Send unknown opcode (0xFF)
        const char invalidCmd = static_cast<char>(0xFF);
        client.write(&invalidCmd, 1);
        client.flush();

        QVERIFY(client.waitForReadyRead(2000));
        QByteArray response = client.readAll();
        QCOMPARE(response.size(), 1);
        QCOMPARE(static_cast<uint8_t>(response[0]), static_cast<uint8_t>(keyinjectord::ResponseStatus::UnknownCmd));

        // Server should immediately disconnect client on unknown opcode
        QVERIFY(client.waitForDisconnected(2000) || client.state() == QLocalSocket::UnconnectedState);
        QCOMPARE(mockDevice.ctrlVCalledCount.load(), 0);

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
};

QTEST_MAIN(TestIpcServer)
#include "test_ipc_server.moc"
