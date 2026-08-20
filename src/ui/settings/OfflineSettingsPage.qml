pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import ".."
import "../controls"

Item {
    id: root

    implicitWidth: 500
    implicitHeight: contentColumn.implicitHeight

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
                text: qsTr("Configure on-device whisper.cpp speech recognition models and hardware acceleration")
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
            title: qsTr("Active Whisper Model")
            description: qsTr("Current model selected for offline transcription inference")

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        spacing: Theme.spacingSm

                        StyledText {
                            text: WhisperModelManager.selectedModelName
                            variant: "body"
                            customWeight: Font.DemiBold
                        }

                        StyledText {
                            text: "(" + WhisperSttClient.modelFileName + ")"
                            variant: "caption"
                            colorRole: "secondary"
                        }
                    }

                    StyledText {
                        text: {
                            if (WhisperSttClient.isModelLoaded && WhisperSttClient.loadedModelPath
                                === WhisperSttClient.modelPath) {
                                return qsTr("Loaded into memory and ready for dictation");
                            }
                            if (WhisperModelManager.isSelectedModelInstalled) {
                                return qsTr("Installed on disk (click Load to activate in memory)");
                            }
                            return qsTr("Model file not installed on disk");
                        }
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StateBadge {
                    text: {
                        if (WhisperSttClient.isModelLoaded && WhisperSttClient.loadedModelPath
                            === WhisperSttClient.modelPath) {
                            return qsTr("Ready");
                        }
                        if (WhisperModelManager.isSelectedModelInstalled) {
                            return qsTr("Installed");
                        }
                        return qsTr("Missing");
                    }
                    statusType: {
                        if (WhisperSttClient.isModelLoaded && WhisperSttClient.loadedModelPath
                            === WhisperSttClient.modelPath) {
                            return "success";
                        }
                        if (WhisperModelManager.isSelectedModelInstalled) {
                            return "neutral";
                        }
                        return "warning";
                    }
                    showDot: true
                    pulsing: WhisperSttClient.busy
                }
            }

            PreferenceDivider {}

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                StyledButton {
                    text: (WhisperSttClient.isModelLoaded && WhisperSttClient.loadedModelPath
                           === WhisperSttClient.modelPath) ? qsTr("Reload Model") : qsTr("Load Model")
                    size: "small"
                    variant: (WhisperSttClient.isModelLoaded && WhisperSttClient.loadedModelPath
                              === WhisperSttClient.modelPath) ? "secondary" : "primary"
                    enabled: WhisperModelManager.isSelectedModelInstalled && !WhisperSttClient.busy
                    onClicked: {
                        WhisperSttClient.loadModel();
                    }
                }

                StyledButton {
                    text: qsTr("Unload Model")
                    size: "small"
                    variant: "secondary"
                    visible: WhisperSttClient.isModelLoaded
                    enabled: !WhisperSttClient.busy
                    onClicked: {
                        WhisperSttClient.unloadModel();
                    }
                }

                StyledButton {
                    text: qsTr("Check Status")
                    size: "small"
                    variant: "flat"
                    onClicked: {
                        WhisperModelManager.refreshModelList();
                        WhisperSttClient.checkModelStatus();
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        PreferenceCard {
            title: qsTr("Whisper Model Library")
            description: qsTr("Download and manage speech recognition models from HuggingFace (ggerganov/whisper.cpp)")

            // Storage and disk space header
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Storage: %1").arg(WhisperModelManager.modelsDirectory)
                        variant: "caption"
                        colorRole: "secondary"
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    StyledText {
                        text: qsTr("Disk Space: %1").arg(WhisperModelManager.availableDiskSpaceFormatted)
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StyledButton {
                    text: qsTr("Open Folder")
                    size: "small"
                    variant: "secondary"
                    onClicked: {
                        Qt.openUrlExternally("file://" + WhisperModelManager.modelsDirectory);
                    }
                }

                StyledButton {
                    text: qsTr("Refresh")
                    size: "small"
                    variant: "flat"
                    onClicked: {
                        WhisperModelManager.refreshModelList();
                    }
                }
            }

            // Error Banner if present
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: errorLayout.implicitHeight + Theme.spacingSm * 2
                radius: Theme.radiusSm
                color: Theme.statusBgColor(Theme.colorDanger, 0.15)
                border.color: Theme.statusBorderColor(Theme.colorDanger, 0.4)
                border.width: 1
                visible: WhisperModelManager.lastError.length > 0

                RowLayout {
                    id: errorLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Theme.spacingSm
                    spacing: Theme.spacingSm

                    StyledText {
                        text: WhisperModelManager.lastError
                        variant: "caption"
                        customColor: Theme.colorDanger
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    StyledButton {
                        text: qsTr("Dismiss")
                        size: "small"
                        variant: "flat"
                        onClicked: {
                            WhisperModelManager.checkDiskSpace();
                        }
                    }
                }
            }

            PreferenceDivider {}

            // Model List
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                Repeater {
                    model: WhisperModelManager

                    delegate: Rectangle {
                        id: modelRow
                        required property int index
                        required property string modelId
                        required property string name
                        required property string fileName
                        required property string downloadUrl
                        required property string sizeFormatted
                        required property string description
                        required property bool isInstalled
                        required property bool isSelected
                        required property bool isDownloading
                        required property real progress
                        required property string speedFormatted
                        required property string installedSizeFormatted
                        required property bool canDelete

                        Layout.fillWidth: true
                        implicitHeight: rowContent.implicitHeight + Theme.spacingMd * 2
                        radius: Theme.radiusMd
                        color: isSelected ? Theme.selectedBg : Theme.cardBgSubtle
                        border.color: isSelected ? Theme.accentColor : Theme.cardBorder
                        border.width: isSelected ? 1 : 1

                        ColumnLayout {
                            id: rowContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.spacingMd
                            spacing: Theme.spacingSm

                            // Title, filename, and badges
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm

                                StyledText {
                                    text: modelRow.name
                                    variant: "body"
                                    customWeight: Font.DemiBold
                                }

                                StyledText {
                                    text: "(" + modelRow.fileName + ")"
                                    variant: "caption"
                                    colorRole: "secondary"
                                }

                                StyledText {
                                    text: modelRow.isInstalled && modelRow.installedSizeFormatted.length > 0
                                          ? modelRow.installedSizeFormatted : modelRow.sizeFormatted
                                    variant: "caption"
                                    colorRole: "secondary"
                                }

                                Item {
                                    Layout.fillWidth: true
                                }

                                StateBadge {
                                    text: qsTr("Active")
                                    statusType: "accent"
                                    visible: modelRow.isSelected
                                }

                                StateBadge {
                                    text: qsTr("Installed")
                                    statusType: "neutral"
                                    visible: modelRow.isInstalled && !modelRow.isDownloading
                                }

                                StateBadge {
                                    text: qsTr("Downloading")
                                    statusType: "accent"
                                    visible: modelRow.isDownloading
                                    showDot: true
                                    pulsing: true
                                }
                            }

                            // Description
                            StyledText {
                                text: modelRow.description
                                variant: "caption"
                                colorRole: "secondary"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            // Active downloading progress bar and speed
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                visible: modelRow.isDownloading

                                RowLayout {
                                    Layout.fillWidth: true

                                    StyledText {
                                        text: qsTr("%1%").arg(Math.round(modelRow.progress * 100))
                                        variant: "caption"
                                        customWeight: Font.DemiBold
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                    StyledText {
                                        text: WhisperModelManager.downloadBytesFormatted + (
                                                  modelRow.speedFormatted.length > 0 ? " (" + modelRow.speedFormatted
                                                                                       + ")" : "")
                                        variant: "caption"
                                        colorRole: "secondary"
                                    }
                                }

                                ProgressBar {
                                    Layout.fillWidth: true
                                    implicitHeight: 6
                                    from: 0.0
                                    to: 1.0
                                    value: Math.min(Math.max(modelRow.progress, 0.0), 1.0)

                                    background: Rectangle {
                                        implicitHeight: 6
                                        radius: Theme.radiusCircle
                                        color: Theme.controlBg
                                    }

                                    contentItem: Item {
                                        implicitHeight: 6
                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            width: parent.width * Math.min(Math.max(modelRow.progress, 0.0), 1.0)
                                            radius: Theme.radiusCircle
                                            color: Theme.accentColor
                                        }
                                    }
                                }
                            }

                            // Action buttons row
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm

                                Item {
                                    Layout.fillWidth: true
                                }

                                // Cancel download button
                                StyledButton {
                                    text: qsTr("Cancel")
                                    size: "small"
                                    variant: "danger"
                                    visible: modelRow.isDownloading
                                    onClicked: {
                                        WhisperModelManager.cancelDownload(modelRow.modelId);
                                    }
                                }

                                // Download button (when not installed & not downloading)
                                StyledButton {
                                    text: qsTr("Download")
                                    size: "small"
                                    variant: "primary"
                                    visible: !modelRow.isInstalled && !modelRow.isDownloading
                                    enabled: !WhisperModelManager.isDownloadingAny
                                    onClicked: {
                                        WhisperModelManager.startDownload(modelRow.modelId);
                                    }
                                }

                                // Set as Active model button (when installed & not selected)
                                StyledButton {
                                    text: qsTr("Set Active")
                                    size: "small"
                                    variant: "secondary"
                                    visible: modelRow.isInstalled && !modelRow.isSelected && !modelRow.isDownloading
                                    onClicked: {
                                        WhisperModelManager.setSelectedModelId(modelRow.modelId);
                                    }
                                }

                                // Delete model button (when installed)
                                StyledButton {
                                    text: qsTr("Delete")
                                    size: "small"
                                    variant: "flat"
                                    visible: modelRow.canDelete && !modelRow.isDownloading
                                    onClicked: {
                                        WhisperModelManager.deleteModel(modelRow.modelId);
                                    }
                                }
                            }
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
