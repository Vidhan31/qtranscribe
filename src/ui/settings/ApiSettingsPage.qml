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
                text: qsTr("API Key")
                variant: "heading"
            }

            StyledText {
                text: qsTr("Manage your Groq API credentials and security")
                variant: "caption"
                colorRole: "secondary"
            }
        }

        PreferenceCard {
            title: qsTr("Groq API Key")
            description: qsTr("Required for speech transcription and text enhancement")

            PreferenceRow {
                title: qsTr("API Key")
                description: qsTr("Enter your API key starting with 'gsk_'")
                layoutVertical: true

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledTextField {
                        id: apiKeyInput
                        Layout.fillWidth: true
                        echoMode: showKeyCheck.checked ? TextField.Normal : TextField.Password
                        text: GroqApiClient.apiKey
                        placeholderText: qsTr("gsk_...")
                    }

                    CheckBox {
                        id: showKeyCheck
                        text: qsTr("Show")
                        font.pixelSize: Theme.fontSizeCaption

                        contentItem: StyledText {
                            text: showKeyCheck.text
                            variant: "caption"
                            colorRole: "secondary"
                            leftPadding: showKeyCheck.indicator.width + Theme.spacingXs
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    StyledButton {
                        id: saveBtn
                        text: qsTr("Save")
                        variant: "primary"
                        size: "medium"
                        onClicked: {
                            GroqApiClient.apiKey = apiKeyInput.text.trim();
                        }
                    }
                }
            }

            PreferenceDivider {}

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                StyledText {
                    text: qsTr("Stored securely in your system keychain.")
                    variant: "caption"
                    colorRole: "secondary"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                StyledButton {
                    id: getKeyBtn
                    text: qsTr("Get API Key")
                    variant: "flat"
                    size: "small"
                    onClicked: {
                        Qt.openUrlExternally("https://console.groq.com/keys");
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Security")
            description: qsTr(
                             "Your secret key is never stored in plain text files. It is encrypted in your system keychain.")

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Keychain Storage")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    StyledText {
                        text: qsTr("FreeDesktop Secret Service / KWallet / GNOME Keyring")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }
            }
        }
    }
}
