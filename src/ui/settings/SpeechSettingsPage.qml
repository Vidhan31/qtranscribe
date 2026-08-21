pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "../controls"

Item {
    id: root

    implicitWidth: 500
    implicitHeight: contentColumn.implicitHeight

    ListModel {
        id: whisperModelsModel
        ListElement {
            name: "Whisper Turbo (Fastest)"
            code: "whisper-large-v3-turbo"
        }
        ListElement {
            name: "Whisper Large V3 (Accurate)"
            code: "whisper-large-v3"
        }
    }

    ListModel {
        id: languageOptionsModel
        ListElement {
            name: "Auto-detect (Default)"
            code: ""
        }
        ListElement {
            name: "English (en)"
            code: "en"
        }
        ListElement {
            name: "Spanish (es)"
            code: "es"
        }
        ListElement {
            name: "Italian (it)"
            code: "it"
        }
        ListElement {
            name: "German (de)"
            code: "de"
        }
        ListElement {
            name: "Portuguese (pt)"
            code: "pt"
        }
        ListElement {
            name: "French (fr)"
            code: "fr"
        }
        ListElement {
            name: "Japanese (ja)"
            code: "ja"
        }
        ListElement {
            name: "Polish (pl)"
            code: "pl"
        }
        ListElement {
            name: "Dutch (nl)"
            code: "nl"
        }
        ListElement {
            name: "Russian (ru)"
            code: "ru"
        }
        ListElement {
            name: "Korean (ko)"
            code: "ko"
        }
        ListElement {
            name: "Catalan (ca)"
            code: "ca"
        }
        ListElement {
            name: "Turkish (tr)"
            code: "tr"
        }
        ListElement {
            name: "Indonesian (id)"
            code: "id"
        }
        ListElement {
            name: "Vietnamese (vi)"
            code: "vi"
        }
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Theme.spacingLg

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingXs
            Layout.bottomMargin: Theme.spacingSm
            spacing: 4

            StyledText {
                text: qsTr("Dictation")
                variant: "heading"
            }

            StyledText {
                text: qsTr("Configure transcription model, spoken language, and audio settings")
                variant: "caption"
                colorRole: "secondary"
            }
        }

        PreferenceCard {
            title: qsTr("Model & Language")
            description: qsTr("Choose the speech recognition model and preferred spoken language")

            PreferenceRow {
                title: qsTr("Model")
                description: qsTr("Turbo delivers maximum speed. Large V3 offers higher accuracy.")
                layoutVertical: true

                StyledComboBox {
                    id: modelCombo
                    Layout.fillWidth: true
                    textRole: "name"
                    valueRole: "code"
                    model: whisperModelsModel
                    currentIndex: {
                        const idx = indexOfValue(GroqSttClient.selectedModel);
                        return idx >= 0 ? idx : 0;
                    }

                    onActivated: index => {
                        GroqSttClient.selectedModel = currentValue;
                    }
                }
            }

            PreferenceDivider {}

            PreferenceRow {
                title: qsTr("Language")
                description: qsTr("Specifying language improves recognition speed and accuracy.")
                layoutVertical: true

                StyledComboBox {
                    id: langCombo
                    Layout.fillWidth: true
                    textRole: "name"
                    valueRole: "code"
                    model: languageOptionsModel
                    currentIndex: {
                        const idx = indexOfValue(GroqSttClient.language);
                        return idx >= 0 ? idx : 0;
                    }

                    onActivated: index => {
                        GroqSttClient.language = currentValue;
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Custom Vocabulary")
            description: qsTr("Teach the transcriber unique names, acronyms, or industry terms.")

            PreferenceRow {
                layoutVertical: true

                RowLayout {
                    Layout.fillWidth: true

                    StyledText {
                        text: qsTr("Custom Words")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledText {
                        text: qsTr("%1 / 800 characters").arg(promptArea.text.length)
                        variant: "caption"
                        customColor: promptArea.text.length > 700 ? Theme.colorWarning : Theme.textSecondary
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    implicitHeight: 85
                    clip: true

                    TextArea {
                        id: promptArea
                        text: GroqSttClient.customPrompt
                        placeholderText: qsTr(
                                             "e.g. QTranscribe, Wayland, Qt6, CMake, JSON, PyTorch, Neovim, kubectl...")
                        placeholderTextColor: Theme.textPlaceholder
                        selectByMouse: true
                        wrapMode: TextArea.Wrap
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.textPrimary

                        background: Rectangle {
                            color: Theme.inputBg
                            border.color: promptArea.activeFocus ? Theme.focusRingColor : Theme.inputBorder
                            border.width: promptArea.activeFocus ? Theme.focusRingWidth : 1
                            radius: Theme.radiusSm
                        }

                        onTextChanged: {
                            if (activeFocus) {
                                savePromptTimer.restart();
                            }
                        }
                    }
                }

                Timer {
                    id: savePromptTimer
                    interval: 400
                    repeat: false
                    onTriggered: {
                        GroqSttClient.customPrompt = promptArea.text;
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledButton {
                        id: clearSttPromptBtn
                        text: qsTr("Clear")
                        enabled: promptArea.text.length > 0
                        size: "small"
                        onClicked: {
                            promptArea.text = "";
                            GroqSttClient.customPrompt = "";
                        }
                    }

                    StyledButton {
                        id: saveSttPromptBtn
                        text: qsTr("Save")
                        size: "small"
                        variant: "primary"
                        onClicked: {
                            savePromptTimer.stop();
                            GroqSttClient.customPrompt = promptArea.text;
                        }
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Sound Effects")
            description: qsTr("Play sounds when dictation starts and stops")

            PreferenceSwitch {
                id: soundSwitch
                title: qsTr("Feedback Sounds")
                description: qsTr("Play sound effects on recording start and completion")
                checked: SpeechController.soundEnabled
                onToggled: {
                    SpeechController.soundEnabled = soundSwitch.checked;
                }
            }

            PreferenceDivider {}

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                opacity: SpeechController.soundEnabled ? 1.0 : 0.5
                enabled: SpeechController.soundEnabled

                StyledText {
                    text: qsTr("Preview:")
                    variant: "caption"
                    colorRole: "secondary"
                }

                Item {
                    Layout.fillWidth: true
                }

                StyledButton {
                    id: playStartBtn
                    text: qsTr("Play Start Sound")
                    size: "small"
                    onClicked: SpeechController.playStartSound()
                }

                StyledButton {
                    id: playStopBtn
                    text: qsTr("Play Stop Sound")
                    size: "small"
                    onClicked: SpeechController.playStopSound()
                }
            }
        }

        PreferenceCard {
            title: qsTr("Microphone Test")
            description: qsTr("Check microphone input level")

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    StyledText {
                        text: qsTr("Input Level:")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        implicitHeight: 8
                        from: 0.0
                        to: 1.0
                        value: Math.min(1.0, Math.max(0.0, AudioRecorder.audioLevel))

                        background: Rectangle {
                            radius: Theme.radiusCircle
                            color: Theme.controlBg
                            border.color: Theme.controlBorder
                            border.width: 1
                        }

                        contentItem: Item {
                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: parent.width * Math.min(1.0, Math.max(0.0, AudioRecorder.audioLevel))
                                radius: Theme.radiusCircle

                                color: {
                                    if (AudioRecorder.audioLevel > 0.85)
                                    return Theme.colorDanger;
                                    if (AudioRecorder.audioLevel > 0.6)
                                    return Theme.colorWarning;
                                    return Theme.colorSuccess;
                                }

                                Behavior on width {
                                    NumberAnimation {
                                        duration: 60
                                    }
                                }
                            }
                        }
                    }

                    StyledText {
                        text: qsTr("%1%").arg(Math.round(AudioRecorder.audioLevel * 100))
                        variant: "caption"
                        customWeight: Font.Bold
                        Layout.preferredWidth: 36
                        horizontalAlignment: Text.AlignRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledText {
                        text: AudioRecorder.recording ? qsTr("Recording active — speak into microphone") : qsTr(
                                                            "Microphone idle")
                        variant: "caption"
                        customColor: AudioRecorder.recording ? Theme.colorSuccess : Theme.textSecondary
                        Layout.fillWidth: true
                    }

                    StyledButton {
                        id: testMicBtn
                        text: AudioRecorder.recording ? qsTr("Stop Test") : qsTr("Test Microphone")
                        enabled: !SpeechController.isBusy || AudioRecorder.recording
                        size: "small"
                        variant: AudioRecorder.recording ? "danger" : "secondary"
                        onClicked: {
                            if (AudioRecorder.recording) {
                                AudioRecorder.cancelRecording();
                            } else {
                                AudioRecorder.startRecording();
                            }
                        }
                    }
                }
            }
        }
    }
}
