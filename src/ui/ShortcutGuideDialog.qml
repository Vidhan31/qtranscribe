pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

Dialog {
    id: root

    title: qsTr("Setup Custom Desktop Shortcut")
    modal: true
    width: 480
    standardButtons: Dialog.Close

    background: Rectangle {
        color: Theme.cardBgElevated
        border.color: Theme.cardBorder
        border.width: 1
        radius: Theme.radiusMd
    }

    header: Rectangle {
        implicitHeight: 44
        color: "transparent"

        StyledText {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            variant: "subheading"
            customWeight: Font.Bold
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingMd

        StyledText {
            text: qsTr(
                      "Your desktop environment does not support the XDG Global Shortcuts portal directly, but you can easily set up a system-wide hotkey using the QTranscribe CLI command:")
            variant: "body"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 38
            color: Theme.inputBg
            border.color: Theme.inputBorder
            border.width: 1
            radius: Theme.radiusSm

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingSm

                StyledText {
                    text: "qtranscribe --toggle"
                    fontFamily: "mono"
                    customWeight: Font.Bold
                    colorRole: "accent"
                    Layout.fillWidth: true
                }

                StyledButton {
                    id: copyCmdBtn
                    text: qsTr("Copy Command")
                    variant: "flat"
                    size: "small"
                    onClicked: {
                        TranscriptionModel.copyToClipboard("qtranscribe --toggle");
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            StyledText {
                text: qsTr("Steps for GNOME:")
                customWeight: Font.DemiBold
                variant: "body"
            }

            StyledText {
                text: qsTr(
                          "1. Open <b>Settings → Keyboard → View and Customize Shortcuts</b>.<br>2. Scroll down to <b>Custom Shortcuts</b> and click <b>+</b>.<br>3. Set Name to <i>QTranscribe</i>, Command to <code>qtranscribe --toggle</code>, and Shortcut to <b>Ctrl+Shift+Space</b>.")
                textFormat: Text.StyledText
                variant: "caption"
                colorRole: "secondary"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
