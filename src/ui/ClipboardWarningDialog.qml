pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

Dialog {
    id: root

    title: qsTr("Important: Clipboard Notice")
    modal: true
    width: 480
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
            text: qsTr("QTranscribe uses clipboard paste injection to insert transcribed speech directly into your active applications on Wayland.")
            variant: "body"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: warningContentLayout.implicitHeight + (Theme.spacingMd * 2)
            color: Theme.statusBgColor(Theme.colorWarning, 0.12)
            border.color: Theme.statusBorderColor(Theme.colorWarning, 0.4)
            border.width: 1
            radius: Theme.radiusSm

            RowLayout {
                id: warningContentLayout
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                spacing: Theme.spacingSm

                StyledIcon {
                    source: "qrc:/qt/qml/QTranscribe/assets/icons/info.svg"
                    color: Theme.colorWarning
                    size: 22
                    Layout.alignment: Qt.AlignTop
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    StyledText {
                        text: qsTr("Clipboard Overwrite Risk")
                        customWeight: Font.DemiBold
                        variant: "body"
                        customColor: Theme.colorWarning
                    }

                    StyledText {
                        text: qsTr("Unless your desktop environment maintains a clipboard history manager (such as a clipboard extension or daemon), transcribing speech may overwrite your currently copied text or media.")
                        variant: "caption"
                        colorRole: "secondary"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        StyledText {
            text: qsTr("Before starting dictation, please make sure you save any important copied content, or consider installing a clipboard manager extension for your desktop environment.")
            variant: "caption"
            colorRole: "secondary"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingSm

            Item {
                Layout.fillWidth: true
            }

            StyledButton {
                id: understandBtn
                text: qsTr("I Understand")
                variant: "primary"
                size: "medium"
                onClicked: {
                    TextInjectorClient.clipboardWarningAcknowledged = true;
                    root.accept();
                }
            }
        }
    }
}
