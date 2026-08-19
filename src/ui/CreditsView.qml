pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

Item {
    id: root

    implicitWidth: 620
    implicitHeight: 540

    ScrollView {
        id: creditsScrollView
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
                    text: qsTr("About")
                    variant: "heading"
                }

                StyledText {
                    text: qsTr("Application information, components, and licenses")
                    variant: "caption"
                    colorRole: "secondary"
                }
            }

            StyledCard {
                Layout.fillWidth: true

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    Image {
                        source: "qrc:/qt/qml/QTranscribe/assets/speech-to-text-64.png"
                        sourceSize: Qt.size(40, 40)
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        Layout.alignment: Qt.AlignVCenter
                        smooth: true
                        mipmap: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        RowLayout {
                            spacing: Theme.spacingSm

                            StyledText {
                                text: qsTr("QTranscribe")
                                variant: "heading"
                            }

                            StateBadge {
                                text: Qt.application.version ? "v" + Qt.application.version : "v1.0.0"
                                statusType: "accent"
                            }
                        }

                        StyledText {
                            text: qsTr("Speech-to-Text dictation client for Wayland.")
                            variant: "body"
                            colorRole: "secondary"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            StyledCard {
                Layout.fillWidth: true

                StyledText {
                    text: qsTr("Components & Libraries")
                    variant: "subheading"
                    Layout.bottomMargin: Theme.spacingXs
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Qt 6 Framework")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: qsTr("Cross-platform application development framework.")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StyledDivider {}

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Groq Cloud API")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: qsTr("Speech-to-text API service for fast audio transcription.")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StyledDivider {}

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("QtKeychain")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: qsTr("Platform-independent Qt API for secure credential storage.")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                StyledDivider {}

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("wl-clipboard")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: qsTr("Command-line copy/paste utilities for Wayland.")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }
            }

            StyledCard {
                Layout.fillWidth: true

                StyledText {
                    text: qsTr("Third-Party Licenses")
                    variant: "subheading"
                    Layout.bottomMargin: Theme.spacingXs
                }

                StyledText {
                    text: '<a href="https://fontawesome.com" title="Font Awesome">Microphone and Mute tray icons by Font Awesome (CC BY 4.0)</a>'
                    textFormat: Text.StyledText
                    variant: "caption"
                    customColor: Theme.accentColor
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }

                StyledText {
                    text: '<a href="https://www.flaticon.com/free-icons/speech-to-text" title="speech to text icons">Speech to text icons created by Fajrul Fitrianto - Flaticon</a>'
                    textFormat: Text.StyledText
                    variant: "caption"
                    customColor: Theme.accentColor
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
        }
    }
}
