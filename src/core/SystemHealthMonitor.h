#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class AbstractSttClient;
class AudioRecorder;
class GlobalShortcutManager;
class TextInjectorClient;

class SystemHealthMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool systemShortcutHasIssue READ systemShortcutHasIssue NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool systemShortcutSupported READ systemShortcutSupported NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(QString systemShortcutStatus READ systemShortcutStatus NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool directTypingHasIssue READ directTypingHasIssue NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool directTypingConnected READ directTypingConnected NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool directTypingFatalError READ directTypingFatalError NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(QString directTypingStatus READ directTypingStatus NOTIFY systemHealthChanged FINAL)
    Q_PROPERTY(bool pushToTalkSupported READ pushToTalkSupported NOTIFY systemHealthChanged FINAL)

public:
    explicit SystemHealthMonitor(QObject* parent = nullptr);
    ~SystemHealthMonitor() override = default;

    void setShortcutManager(GlobalShortcutManager* mgr);
    void setTextInjector(TextInjectorClient* injector);
    void setAudioRecorder(AudioRecorder* recorder);
    void setActiveSttClient(AbstractSttClient* client);

    bool systemShortcutHasIssue() const;
    bool systemShortcutSupported() const;
    QString systemShortcutStatus() const;

    bool directTypingHasIssue() const;
    bool directTypingConnected() const;
    bool directTypingFatalError() const;
    QString directTypingStatus() const;

    bool pushToTalkSupported() const;
    bool canRecord(bool notProcessing) const;

public slots:
    void notifyHealthChanged();

signals:
    void systemHealthChanged();
    void canRecordChanged();

private:
    GlobalShortcutManager* m_shortcutMgr = nullptr;
    TextInjectorClient* m_injector = nullptr;
    AudioRecorder* m_recorder = nullptr;
    AbstractSttClient* m_activeSttClient = nullptr;
};
