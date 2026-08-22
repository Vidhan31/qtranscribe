pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QTranscribe
import "../controls"

Item {
    id: root

    implicitWidth: 500
    implicitHeight: contentColumn.implicitHeight

    ListModel {
        id: themeOptionsModel
        ListElement {
            name: "System (Auto)"
            code: "system"
        }
        ListElement {
            name: "Dark Theme"
            code: "dark"
        }
        ListElement {
            name: "Light Theme"
            code: "light"
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
                text: qsTr("System & Typing")
                variant: "heading"
            }

            StyledText {
                text: qsTr("Configure theme, direct typing, clipboard privacy, and keyboard shortcuts")
                variant: "caption"
                colorRole: "secondary"
            }
        }

        PreferenceCard {
            title: qsTr("Appearance")
            description: qsTr("Choose your preferred theme or follow your desktop environment")

            PreferenceRow {
                title: qsTr("Theme")
                description: qsTr("System detected: %1 mode").arg(Theme.systemThemeName)
                layoutVertical: false

                StyledComboBox {
                    id: themeCombo
                    Layout.preferredWidth: 180
                    textRole: "name"
                    valueRole: "code"
                    model: themeOptionsModel
                    currentIndex: {
                        const idx = indexOfValue(Theme.themeMode);
                        return idx >= 0 ? idx : 0;
                    }

                    onActivated: index => {
                        Theme.themeMode = currentValue;
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Direct Typing")
            description: qsTr("Types text directly into the active application")

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        StyledText {
                            text: qsTr("Service Status")
                            variant: "body"
                            customWeight: Font.Medium
                        }

                        StyledText {
                            text: TextInjectorClient.hasFatalError ? (TextInjectorClient.fatalErrorMessage.length > 0
                                                                      ? TextInjectorClient.fatalErrorMessage : qsTr(
                                                                            "Service failed to start")) : (
                                                                         TextInjectorClient.statusMessage.length > 0
                                                                         ? TextInjectorClient.statusMessage : (
                                                                               TextInjectorClient.connected ? qsTr(
                                                                                                                  "Connected to background typing service") :
                                                                                                              qsTr("Not running — using clipboard fallback")))
                            variant: "caption"
                            customColor: TextInjectorClient.connected ? Theme.colorSuccess : (
                                                                            TextInjectorClient.hasFatalError
                                                                            ? Theme.colorDanger : Theme.textSecondary)
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                PreferenceDivider {}

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledButton {
                        id: testTypingBtn
                        text: qsTr("Test Typing")
                        enabled: TextInjectorClient.connected
                        size: "small"
                        onClicked: {
                            TextInjectorClient.typeText(" [QTranscribe Test] ");
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledButton {
                        id: restartServiceBtn
                        text: TextInjectorClient.hasFatalError ? qsTr("Restart Service") : (
                                                                     TextInjectorClient.connected ? qsTr(
                                                                                                        "Restart Service") :
                                                                                                    qsTr("Connect Service"))
                        variant: "primary"
                        size: "small"
                        onClicked: {
                            TextInjectorClient.restartService();
                        }
                    }

                    StyledButton {
                        id: stopServiceBtn
                        text: qsTr("Stop Service")
                        enabled: TextInjectorClient.connected
                        variant: "danger"
                        size: "small"
                        onClicked: {
                            TextInjectorClient.stopDaemon();
                        }
                    }
                }

                PreferenceDivider {}

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            StyledText {
                                text: qsTr("Pre-Injection Delay")
                                variant: "body"
                                customWeight: Font.Medium
                            }

                            StyledText {
                                text: qsTr("Wait before injecting text into the active application")
                                variant: "caption"
                                colorRole: "secondary"
                            }
                        }

                        StyledText {
                            text: TextInjectorClient.injectionDelay === 0 ? qsTr("0 ms (Immediate)") : qsTr("%1 ms").arg(
                                                                                TextInjectorClient.injectionDelay)

                            variant: "body"
                            customWeight: Font.Bold
                            colorRole: "accent"
                            Layout.preferredWidth: 120
                            horizontalAlignment: Text.AlignRight
                        }

                        StyledButton {
                            id: resetDelayBtn
                            text: qsTr("Reset")
                            variant: "flat"
                            size: "small"
                            enabled: TextInjectorClient.injectionDelay !== 200
                            onClicked: {
                                TextInjectorClient.injectionDelay = 200;
                            }
                        }
                    }

                    StyledSlider {
                        id: delaySlider
                        Layout.fillWidth: true
                        from: 0
                        to: 2000
                        stepSize: 50
                        value: TextInjectorClient.injectionDelay
                        onMoved: {
                            TextInjectorClient.injectionDelay = Math.round(value);
                        }
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Clipboard Privacy & Behavior")
            description: qsTr("Control clipboard privacy and text injection behavior on Wayland")

            PreferenceSwitch {
                id: clipboardHistorySwitch
                title: qsTr("Exclude from Clipboard History")
                description: qsTr(
                                 "Prevents clipboard managers (KDE Klipper, GNOME GPaste) from storing your dictated text")
                checked: TextInjectorClient.preventClipboardHistory
                onToggled: {
                    TextInjectorClient.preventClipboardHistory = clipboardHistorySwitch.checked;
                }
            }

            PreferenceDivider {
                visible: !TextInjectorClient.isKde
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                visible: !TextInjectorClient.isKde

                StyledText {
                    text: qsTr("Wayland Clipboard Guidance")
                    variant: "body"
                    customWeight: Font.Medium
                }

                StyledText {
                    text: qsTr(
                              "On desktop environments without integrated clipboard history (like GNOME or wlroots compositors), direct typing injects text via clipboard paste. This may overwrite your copied data unless a clipboard manager extension is active.")
                    variant: "caption"
                    colorRole: "secondary"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    StyledButton {
                        id: resetWarningBtn
                        text: qsTr("Reset Clipboard Warning")
                        variant: "flat"
                        size: "small"
                        onClicked: {
                            TextInjectorClient.resetClipboardWarning();
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Global Shortcut")
            description: qsTr("System-wide keyboard shortcut to start and stop dictation")

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        StyledText {
                            text: qsTr("Portal Status")
                            variant: "body"
                            customWeight: Font.Medium
                        }

                        StyledText {
                            text: GlobalShortcutManager.statusMessage.length > 0 ? GlobalShortcutManager.statusMessage :
                                                                                   qsTr("Registered via Desktop Portal (Ctrl+Shift+Space)")
                            variant: "caption"
                            customColor: GlobalShortcutManager.available ? Theme.colorSuccess : (
                                                                               GlobalShortcutManager.supported
                                                                               ? Theme.colorWarning :
                                                                                 Theme.textSecondary)
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                PreferenceDivider {}

                StyledCard {
                    Layout.fillWidth: true
                    variant: "subtle"
                    padding: Theme.spacingMd

                    ColumnLayout {
                        id: customShortcutColumn
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm

                        StyledText {
                            text: qsTr("Desktop Environment Custom Shortcut (CLI)")
                            variant: "caption"
                            customWeight: Font.DemiBold
                        }

                        StyledText {
                            text: qsTr(
                                      "If your desktop does not support portal shortcuts (e.g. GNOME, Sway), bind this command to <b>Ctrl+Shift+Space</b> in your desktop's keyboard settings:")
                            textFormat: Text.StyledText
                            variant: "caption"
                            colorRole: "secondary"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 32
                                color: Theme.inputBg
                                border.color: Theme.inputBorder
                                border.width: 1
                                radius: Theme.radiusSm

                                StyledText {
                                    anchors.centerIn: parent
                                    text: "qtranscribe --toggle"
                                    fontFamily: "mono"
                                    variant: "caption"
                                    customWeight: Font.Bold
                                    colorRole: "accent"
                                }
                            }

                            StyledButton {
                                id: copyCliBtn
                                text: qsTr("Copy")
                                size: "small"
                                onClicked: {
                                    TranscriptionModel.copyToClipboard("qtranscribe --toggle");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
