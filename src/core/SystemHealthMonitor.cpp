#include "SystemHealthMonitor.h"

#include "AudioRecorder.h"
#include "TextInjectorClient.h"

#include "AbstractSttClient.h"
#include "GlobalShortcutManager.h"

SystemHealthMonitor::SystemHealthMonitor(QObject* parent)
    : QObject(parent) { }

void SystemHealthMonitor::setShortcutManager(GlobalShortcutManager* mgr) {
    if (m_shortcutMgr == mgr) {
        return;
    }
    if (m_shortcutMgr) {
        disconnect(m_shortcutMgr, &GlobalShortcutManager::availableChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
        disconnect(m_shortcutMgr, &GlobalShortcutManager::supportedChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
        disconnect(m_shortcutMgr, &GlobalShortcutManager::statusMessageChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
    }
    m_shortcutMgr = mgr;
    if (m_shortcutMgr) {
        connect(m_shortcutMgr, &GlobalShortcutManager::availableChanged, this,
                &SystemHealthMonitor::notifyHealthChanged);
        connect(m_shortcutMgr, &GlobalShortcutManager::supportedChanged, this,
                &SystemHealthMonitor::notifyHealthChanged);
        connect(m_shortcutMgr, &GlobalShortcutManager::statusMessageChanged, this,
                &SystemHealthMonitor::notifyHealthChanged);
    }
    notifyHealthChanged();
}

void SystemHealthMonitor::setTextInjector(TextInjectorClient* injector) {
    if (m_injector == injector) {
        return;
    }
    if (m_injector) {
        disconnect(m_injector, &TextInjectorClient::connectedChanged, this, &SystemHealthMonitor::notifyHealthChanged);
        disconnect(m_injector, &TextInjectorClient::hasFatalErrorChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
        disconnect(m_injector, &TextInjectorClient::fatalErrorMessageChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
    }
    m_injector = injector;
    if (m_injector) {
        connect(m_injector, &TextInjectorClient::connectedChanged, this, &SystemHealthMonitor::notifyHealthChanged);
        connect(m_injector, &TextInjectorClient::hasFatalErrorChanged, this, &SystemHealthMonitor::notifyHealthChanged);
        connect(m_injector, &TextInjectorClient::fatalErrorMessageChanged, this,
                &SystemHealthMonitor::notifyHealthChanged);
    }
    notifyHealthChanged();
}

void SystemHealthMonitor::setAudioRecorder(AudioRecorder* recorder) {
    if (m_recorder == recorder) {
        return;
    }
    if (m_recorder) {
        disconnect(m_recorder, &AudioRecorder::hasAudioInputDeviceChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
    }
    m_recorder = recorder;
    if (m_recorder) {
        connect(m_recorder, &AudioRecorder::hasAudioInputDeviceChanged, this,
                &SystemHealthMonitor::notifyHealthChanged);
    }
    notifyHealthChanged();
}

void SystemHealthMonitor::setActiveSttClient(AbstractSttClient* client) {
    if (m_activeSttClient == client) {
        return;
    }
    if (m_activeSttClient) {
        disconnect(m_activeSttClient, &AbstractSttClient::readyChanged, this,
                   &SystemHealthMonitor::notifyHealthChanged);
        disconnect(m_activeSttClient, &AbstractSttClient::busyChanged, this, &SystemHealthMonitor::notifyHealthChanged);
    }
    m_activeSttClient = client;
    if (m_activeSttClient) {
        connect(m_activeSttClient, &AbstractSttClient::readyChanged, this, &SystemHealthMonitor::notifyHealthChanged);
        connect(m_activeSttClient, &AbstractSttClient::busyChanged, this, &SystemHealthMonitor::notifyHealthChanged);
    }
    notifyHealthChanged();
}

bool SystemHealthMonitor::systemShortcutHasIssue() const {
    return m_shortcutMgr && !m_shortcutMgr->isAvailable();
}

bool SystemHealthMonitor::systemShortcutSupported() const {
    return m_shortcutMgr && m_shortcutMgr->isSupported();
}

QString SystemHealthMonitor::systemShortcutStatus() const {
    return m_shortcutMgr ? m_shortcutMgr->statusMessage() : QString();
}

bool SystemHealthMonitor::directTypingHasIssue() const {
    return m_injector && (!m_injector->isConnected() || m_injector->hasFatalError());
}

bool SystemHealthMonitor::directTypingConnected() const {
    return m_injector && m_injector->isConnected();
}

bool SystemHealthMonitor::directTypingFatalError() const {
    return m_injector && m_injector->hasFatalError();
}

QString SystemHealthMonitor::directTypingStatus() const {
    if (!m_injector) {
        return QString();
    }
    if (m_injector->hasFatalError()) {
        return m_injector->fatalErrorMessage().isEmpty() ? tr("Direct Typing Error") : m_injector->fatalErrorMessage();
    }
    if (!m_injector->isConnected()) {
        return tr("Clipboard Fallback");
    }
    return tr("Connected");
}

bool SystemHealthMonitor::pushToTalkSupported() const {
    return m_shortcutMgr && m_shortcutMgr->isSupported();
}

bool SystemHealthMonitor::canRecord(bool notProcessing) const {
    const bool micReady = m_recorder && m_recorder->hasAudioInputDevice();
    const bool sttReady = m_activeSttClient && m_activeSttClient->isReady();
    return micReady && notProcessing && sttReady;
}

void SystemHealthMonitor::notifyHealthChanged() {
    emit systemHealthChanged();
    emit canRecordChanged();
}
