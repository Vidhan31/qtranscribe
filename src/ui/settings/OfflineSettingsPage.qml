pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QTranscribe
import ".."
import "../controls"

Item {
    id: root

    implicitWidth: 500
    implicitHeight: contentColumn.implicitHeight

    property bool showCopyFeedback: false

    Timer {
        id: copyFeedbackTimer
        interval: 2000
        repeat: false
        onTriggered: {
            root.showCopyFeedback = false;
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
                text: qsTr("Offline Dictation")
                variant: "heading"
            }

            StyledText {
                text: qsTr("Configure on-device whisper.cpp transcription and hardware acceleration")
                variant: "caption"
                colorRole: "secondary"
            }
        }

        PreferenceCard {
            title: qsTr("Engine Selection")
            description: qsTr("Choose between cloud-based Groq transcription and fully offline local inference")

            PreferenceSwitch {
                id: offlineSwitch
                title: qsTr("Use Offline Whisper Engine")
                description: qsTr("Transcribe audio locally on your device without sending voice data to the cloud")
                checked: SpeechController.activeBackend === SpeechController.TranscriptionBackend.WhisperCpp
                onToggled: {
                    if (checked) {
                        SpeechController.activeBackend = SpeechController.TranscriptionBackend.WhisperCpp;
                    } else {
                        SpeechController.activeBackend = SpeechController.TranscriptionBackend.Groq;
                    }
                }
            }

            PreferenceDivider {}

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                StyledText {
                    text: qsTr("Active Provider:")
                    variant: "caption"
                    colorRole: "secondary"
                }

                StateBadge {
                    text: SpeechController.activeBackend === SpeechController.TranscriptionBackend.WhisperCpp ? qsTr(
                                                                                                                    "Local (whisper.cpp)") :
                                                                                                                qsTr("Cloud (Groq Whisper)")
                    statusType: SpeechController.activeBackend === SpeechController.TranscriptionBackend.WhisperCpp
                                ? "accent" : "success"
                    showDot: true
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        PreferenceCard {
            title: qsTr("Whisper Model (tiny.en)")
            description: qsTr("High-speed, lightweight English transcription model (~75 MiB)")

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Model Status")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: WhisperSttClient.isModelLoaded ? qsTr("Loaded into memory and ready for dictation") : (
                                                                   WhisperSttClient.isModelInstalled ? qsTr(
                                                                                                           "Model file installed on disk") :
                                                                                                       qsTr("Model file not found"))
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StateBadge {
                    text: {
                        if (WhisperSttClient.isModelLoaded)
                        return qsTr("Ready");
                        if (WhisperSttClient.isModelInstalled)
                        return qsTr("Installed");
                        return qsTr("Missing");
                    }
                    statusType: {
                        if (WhisperSttClient.isModelLoaded)
                        return "success";
                        if (WhisperSttClient.isModelInstalled)
                        return "neutral";
                        return "warning";
                    }
                    showDot: true
                    pulsing: WhisperSttClient.busy
                }
            }

            PreferenceDivider {}

            PreferenceRow {
                title: qsTr("Storage Location")
                description: WhisperSttClient.modelPath
                layoutVertical: true

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledButton {
                        text: qsTr("Open Models Folder")
                        size: "small"
                        variant: "secondary"
                        onClicked: {
                            Qt.openUrlExternally("file://" + WhisperSttClient.modelsDirectory);
                        }
                    }

                    StyledButton {
                        text: qsTr("Check Status")
                        size: "small"
                        variant: "flat"
                        onClicked: {
                            WhisperSttClient.checkModelStatus();
                            if (SpeechController.activeBackend === SpeechController.TranscriptionBackend.WhisperCpp) {
                                WhisperSttClient.loadModel();
                            }
                        }
                    }
                }
            }

            PreferenceDivider {
                visible: !WhisperSttClient.isModelInstalled
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                visible: !WhisperSttClient.isModelInstalled

                StyledText {
                    text: qsTr("Download Instructions")
                    variant: "body"
                    customWeight: Font.Medium
                    customColor: Theme.colorWarning
                }

                StyledText {
                    text: qsTr(
                              "To enable offline transcription, download %1 and place it in the models directory shown above.").arg(
                              WhisperSttClient.modelFileName)
                    variant: "caption"
                    colorRole: "secondary"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledButton {
                        text: qsTr("Download Model File")
                        size: "small"
                        variant: "primary"
                        onClicked: {
                            Qt.openUrlExternally(WhisperSttClient.downloadUrl);
                        }
                    }

                    StyledButton {
                        text: root.showCopyFeedback ? qsTr("Command Copied!") : qsTr("Copy curl Command")
                        size: "small"
                        variant: "secondary"
                        onClicked: {
                            const cmd = "curl -L -o \"" + WhisperSttClient.modelsDirectory + "/"
                            + WhisperSttClient.modelFileName + "\" \"" + WhisperSttClient.downloadUrl + "\"";
                            ClipboardManager.setText(cmd);
                            root.showCopyFeedback = true;
                            copyFeedbackTimer.restart();
                        }
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Hardware Acceleration")
            description: qsTr("GPU compute offloading status and inference backend diagnostics")

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Inference Engine Device")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: WhisperSttClient.isVulkanSupported ? qsTr(
                                                                       "Vulkan GPU acceleration is enabled in this build") :
                                                                   qsTr("Running via optimized CPU SIMD intrinsics (AVX2/AVX)")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StateBadge {
                    text: WhisperSttClient.computeDevice
                    statusType: WhisperSttClient.isVulkanSupported ? "accent" : "neutral"
                }
            }
        }
    }
}
