pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

Dialog {
    id: root

    title: qsTr("Clipboard Overwrite Notice")
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

    contentItem: ScrollView {
        id: scrollView
        implicitHeight: Math.min(warningLayout.implicitHeight, 420)
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: warningLayout
            width: scrollView.availableWidth
            spacing: Theme.spacingMd

            StyledText {
                text: qsTr(
                          "QTranscribe uses clipboard paste injection to insert transcribed speech directly into your active applications on Wayland.")
                variant: "body"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cardContentColumn.implicitHeight + (Theme.spacingMd * 2)
                color: Theme.cardBgSubtle
                border.color: Theme.cardBorder
                border.width: 1
                radius: Theme.radiusSm

                ColumnLayout {
                    id: cardContentColumn
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    spacing: Theme.spacingMd

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.spacingSm

                        StyledIcon {
                            source: "qrc:/qt/qml/QTranscribe/assets/icons/check.svg"
                            color: Theme.colorSuccess
                            size: 18
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: 2

                            StyledText {
                                text: qsTr("Text is Preserved")
                                customWeight: Font.DemiBold
                                variant: "body"
                                customColor: Theme.colorSuccess
                            }

                            StyledText {
                                text: qsTr(
                                          "Copied plain text is automatically backed up and restored immediately after dictation paste completes.")
                                variant: "caption"
                                colorRole: "secondary"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Theme.cardBorder
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.spacingSm

                        StyledIcon {
                            source: "qrc:/qt/qml/QTranscribe/assets/icons/info.svg"
                            color: Theme.colorWarning
                            size: 18
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: 2

                            StyledText {
                                text: qsTr("Images & Media are Overwritten")
                                customWeight: Font.DemiBold
                                variant: "body"
                                customColor: Theme.colorWarning
                            }

                            StyledText {
                                text: qsTr(
                                          "Non-text clipboard formats (such as images, screenshots, or copied files) cannot be restored and will be overwritten.")
                                variant: "caption"
                                colorRole: "secondary"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            StyledText {
                text: qsTr(
                          "Before transcribing, please paste and save any important copied images or files. Alternatively, consider installing a desktop clipboard manager extension (such as Clipboard Indicator on GNOME).")
                variant: "caption"
                colorRole: "secondary"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 52
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd

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
