pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

Item {
    id: root

    signal openSettingsRequested(int categoryIndex)

    implicitWidth: 620
    implicitHeight: 540

    property bool showCopySuccess: false

    Timer {
        id: copyFeedbackTimer
        interval: 2000
        repeat: false
        onTriggered: {
            root.showCopySuccess = false;
        }
    }

    Connections {
        target: SpeechController
        function onOpenSettingsRequested(categoryIndex: int) {
            root.openSettingsRequested(categoryIndex);
        }
    }

    ShortcutGuideDialog {
        id: shortcutGuideDialog
    }

    ScrollView {
        id: speechScrollView
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingMd

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXs
                Layout.bottomMargin: Theme.spacingSm
                spacing: 4

                StyledText {
                    text: qsTr("Dictate")
                    variant: "heading"
                }

                StyledText {
                    text: qsTr("Real-time voice dictation and transcription pad")
                    variant: "caption"
                    colorRole: "secondary"
                }
            }

            StatusBanner {
                visible: SpeechController.hasActiveNotice
                bannerType: SpeechController.activeNotice.type || "info"
                title: SpeechController.activeNotice.title || ""
                message: SpeechController.activeNotice.message || ""
                actionText: SpeechController.activeNotice.actionText || ""
                onActionClicked: {
                    SpeechController.triggerNoticeAction(SpeechController.activeNotice.actionId);
                }
                secondaryActionText: SpeechController.activeNotice.secondaryActionText || ""
                onSecondaryActionClicked: {
                    SpeechController.triggerNoticeAction(SpeechController.activeNotice.secondaryActionId);
                }
            }

            StyledCard {
                Layout.fillWidth: true
                customRadius: Theme.radiusMd
                customBorderColor: SpeechController.recording ? Theme.colorDanger : Theme.cardBorder
                customBorderWidth: SpeechController.recording ? 2 : 1
                padding: Theme.spacingLg

                Behavior on customBorderColor {
                    ColorAnimation {
                        duration: Theme.animNormal
                    }
                }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 96
                    implicitHeight: 96

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: Theme.colorDanger
                        opacity: SpeechController.recording ? 0.25 : 0.0
                        visible: SpeechController.recording

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Theme.animNormal
                            }
                        }

                        SequentialAnimation on scale {
                            running: SpeechController.recording
                            loops: Animation.Infinite
                            ScaleAnimator {
                                to: 1.18
                                duration: 650
                            }
                            ScaleAnimator {
                                to: 1.0
                                duration: 650
                            }
                        }
                    }

                    Button {
                        id: recordBtn
                        anchors.fill: parent
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: SpeechController.recording ? qsTr("Stop Dictation") : qsTr("Start Dictation")
                        Accessible.description: qsTr("Toggle speech recognition recording")
                        enabled: SpeechController.canRecord

                        background: Rectangle {
                            radius: recordBtn.width / 2
                            color: {
                                if (!recordBtn.enabled)
                                return Theme.controlBg;
                                if (SpeechController.recording)
                                return recordBtn.down ? Theme.buttonDangerBgPressed : (recordBtn.hovered
                                                                                       ? Theme.buttonDangerBgHover :
                                                                                         Theme.buttonDangerBg);
                                if (SpeechController.isBusy)
                                return Theme.colorWarning;
                                return recordBtn.down ? Theme.buttonPrimaryBgPressed : (recordBtn.hovered
                                                                                        ? Theme.buttonPrimaryBgHover :
                                                                                          Theme.buttonPrimaryBg);
                            }
                            border.color: recordBtn.visualFocus ? Theme.focusRingColor : Theme.cardBorder
                            border.width: recordBtn.visualFocus ? Theme.focusRingWidth : 1

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.animFast
                                }
                            }
                        }

                        contentItem: ColumnLayout {
                            spacing: 2
                            anchors.centerIn: parent

                            Item {
                                Layout.alignment: Qt.AlignHCenter
                                implicitWidth: 28
                                implicitHeight: 28

                                Image {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    source: "qrc:/qt/qml/QTranscribe/assets/icons/mic.svg"
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    visible: !SpeechController.recording && !SpeechController.isBusy
                                    smooth: true
                                }

                                Image {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    source: "qrc:/qt/qml/QTranscribe/assets/icons/stop.svg"
                                    sourceSize.width: 22
                                    sourceSize.height: 22
                                    visible: SpeechController.recording
                                    smooth: true
                                }

                                Image {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    source: "qrc:/qt/qml/QTranscribe/assets/icons/spinner.svg"
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                    visible: SpeechController.isBusy
                                    smooth: true

                                    RotationAnimation on rotation {
                                        running: SpeechController.isBusy
                                        loops: Animation.Infinite
                                        from: 0
                                        to: 360
                                        duration: 1000
                                    }
                                }
                            }

                            StyledText {
                                text: SpeechController.recording ? qsTr("Stop") : (SpeechController.isBusy ? qsTr(
                                                                                                                 "Processing…") :
                                                                                                             qsTr("Record"))
                                variant: "caption"
                                customWeight: Font.DemiBold
                                colorRole: "onAccent"
                                Layout.alignment: Qt.AlignHCenter
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        onClicked: {
                            SpeechController.toggleRecording();
                        }
                    }
                }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 120
                    implicitHeight: 32
                    visible: SpeechController.recording

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5

                        Repeater {
                            model: 7
                            delegate: Item {
                                id: waveformBar
                                required property int index
                                Layout.preferredWidth: 4
                                Layout.preferredHeight: 28

                                readonly property real multiplier: {
                                    switch (index) {
                                        case 0:
                                        case 6:
                                        return 0.5;
                                        case 1:
                                        case 5:
                                        return 0.8;
                                        case 2:
                                        case 4:
                                        return 1.1;
                                        default:
                                        return 1.4;
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 4
                                    height: Math.max(4, 28 * Math.min(AudioRecorder.audioLevel * waveformBar.multiplier,
                                                                      1.0))

                                    radius: Theme.radiusXs / 2
                                    color: Theme.colorSuccess

                                    Behavior on height {
                                        NumberAnimation {
                                            duration: 50
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                StyledText {
                    id: audioStatusText
                    text: SpeechController.statusMessage
                    variant: "caption"
                    colorRole: "secondary"
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    visible: audioStatusText.text.length > 0
                }

                Rectangle {
                    id: systemStatusPill
                    visible: SpeechController.systemShortcutHasIssue || SpeechController.directTypingHasIssue
                    Layout.alignment: Qt.AlignHCenter
                    implicitHeight: 32
                    implicitWidth: footerStatusRow.implicitWidth + (Theme.spacingMd * 2)
                    radius: Theme.radiusCircle
                    color: Theme.cardBgSubtle
                    border.color: Theme.cardBorder
                    border.width: 1

                    RowLayout {
                        id: footerStatusRow
                        anchors.centerIn: parent
                        spacing: Theme.spacingMd

                        RowLayout {
                            id: shortcutStatusRow
                            visible: SpeechController.systemShortcutHasIssue
                            spacing: 6

                            StyledIcon {
                                source: "qrc:/qt/qml/QTranscribe/assets/icons/keyboard.svg"
                                size: 14
                                color: Theme.textSecondary
                                Layout.alignment: Qt.AlignVCenter
                                opacity: 0.8
                            }

                            Rectangle {
                                implicitWidth: 6
                                implicitHeight: 6
                                radius: 3
                                Layout.alignment: Qt.AlignVCenter
                                color: SpeechController.systemShortcutSupported ? Theme.colorWarning : Theme.colorDanger
                            }

                            StyledText {
                                text: SpeechController.systemShortcutStatus.length > 0
                                      ? SpeechController.systemShortcutStatus : qsTr("Global shortcut unavailable")
                                variant: "caption"
                                colorRole: "secondary"
                                Layout.alignment: Qt.AlignVCenter
                            }

                            StyledButton {
                                id: setupGuideBtn
                                text: qsTr("Setup Guide")
                                visible: !SpeechController.systemShortcutSupported
                                variant: "flat"
                                size: "small"
                                onClicked: shortcutGuideDialog.open()
                            }
                        }

                        StyledDivider {
                            id: statusDivider
                            visible: SpeechController.systemShortcutHasIssue && SpeechController.directTypingHasIssue
                            orientation: Qt.Vertical
                            implicitHeight: 14
                            Layout.alignment: Qt.AlignVCenter
                        }

                        RowLayout {
                            id: directTypingStatusRow
                            visible: SpeechController.directTypingHasIssue
                            spacing: 6

                            StyledIcon {
                                source: "qrc:/qt/qml/QTranscribe/assets/icons/bolt.svg"
                                size: 13
                                color: Theme.textSecondary
                                Layout.alignment: Qt.AlignVCenter
                                opacity: 0.8
                            }

                            Rectangle {
                                implicitWidth: 6
                                implicitHeight: 6
                                radius: 3
                                Layout.alignment: Qt.AlignVCenter
                                color: SpeechController.directTypingFatalError ? Theme.colorDanger : Theme.colorWarning
                            }

                            StyledText {
                                text: SpeechController.directTypingStatus
                                variant: "caption"
                                colorRole: "secondary"
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }
                    }
                }
            }

            StyledCard {
                Layout.fillWidth: true
                customRadius: Theme.radiusMd
                padding: Theme.spacingMd

                RowLayout {
                    Layout.fillWidth: true

                    StyledText {
                        text: qsTr("Dictation Pad")
                        variant: "body"
                        customWeight: Font.DemiBold
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledText {
                        text: qsTr("%1 words • %2 characters").arg(SpeechController.dictationWordCount).arg(
                                  SpeechController.dictationCharCount)
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    implicitHeight: 96
                    clip: true

                    TextArea {
                        id: testingGroundText
                        text: SpeechController.dictationPadText
                        placeholderText: qsTr("Text will appear here when dictating…")
                        placeholderTextColor: Theme.textPlaceholder
                        font.pixelSize: Theme.fontSizeBody
                        wrapMode: Text.WordWrap
                        selectByMouse: true
                        color: Theme.textPrimary

                        background: Rectangle {
                            color: Theme.inputBg
                            border.color: testingGroundText.activeFocus ? Theme.focusRingColor : Theme.inputBorder
                            border.width: testingGroundText.activeFocus ? Theme.focusRingWidth : 1
                            radius: Theme.radiusSm
                        }

                        onTextChanged: {
                            if (activeFocus) {
                                SpeechController.dictationPadText = text;
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledButton {
                        id: copyPadBtn
                        text: root.showCopySuccess ? qsTr("Copied!") : qsTr("Copy")
                        iconSource: root.showCopySuccess ? "qrc:/qt/qml/QTranscribe/assets/icons/check.svg" :
                                                           "qrc:/qt/qml/QTranscribe/assets/icons/copy.svg"
                        enabled: SpeechController.dictationCharCount > 0
                        size: "small"
                        variant: root.showCopySuccess ? "primary" : "secondary"
                        onClicked: {
                            SpeechController.copyDictationPad();
                            root.showCopySuccess = true;
                            copyFeedbackTimer.restart();
                        }
                    }

                    StyledButton {
                        id: clearPadBtn
                        text: qsTr("Clear")
                        iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/trash.svg"
                        enabled: SpeechController.dictationCharCount > 0
                        size: "small"
                        onClicked: {
                            SpeechController.clearDictationPad();
                        }
                    }
                }
            }
        }
    }
}
