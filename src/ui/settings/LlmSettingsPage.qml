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
        id: llmModelOptionsModel
        ListElement {
            name: "GPT-OSS 20B (Recommended)"
            code: "openai/gpt-oss-20b"
        }
        ListElement {
            name: "GPT-OSS 120B (High Depth)"
            code: "openai/gpt-oss-120b"
        }
        ListElement {
            name: "LLaMA 3.3 70B (Versatile)"
            code: "llama-3.3-70b-versatile"
        }
        ListElement {
            name: "Qwen 3.6 27B (Multilingual)"
            code: "qwen/qwen3.6-27b"
        }
        ListElement {
            name: "LLaMA 3.1 8B (Fast)"
            code: "llama-3.1-8b-instant"
        }
    }

    ListModel {
        id: presetOptionsModel
        ListElement {
            name: "Fix Grammar & Typos"
            code: "grammar"
        }
        ListElement {
            name: "Format as Bullet Points"
            code: "bullets"
        }
        ListElement {
            name: "Professional Tone"
            code: "professional"
        }
        ListElement {
            name: "Custom Instructions"
            code: "custom"
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
                text: qsTr("Text Enhancement")
                variant: "heading"
            }

            StyledText {
                text: qsTr("Automatically polish, reformat, or fix grammar after dictation")
                variant: "caption"
                colorRole: "secondary"
            }
        }

        PreferenceCard {
            title: qsTr("Text Enhancement")
            description: qsTr("Automatically refine voice transcription before typing or saving")

            PreferenceSwitch {
                id: enableSwitch
                title: qsTr("Enable Text Enhancement")
                description: qsTr("Apply style formatting and corrections to transcribed text")
                checked: GroqLlmClient.enabled
                onToggled: {
                    GroqLlmClient.enabled = enableSwitch.checked;
                }
            }

            PreferenceDivider {}

            PreferenceRow {
                title: qsTr("Model")
                description: qsTr("Language model used for text processing")
                layoutVertical: true
                opacity: GroqLlmClient.enabled ? 1.0 : 0.5
                enabled: GroqLlmClient.enabled

                StyledComboBox {
                    id: llmModelCombo
                    Layout.fillWidth: true
                    textRole: "name"
                    valueRole: "code"
                    model: llmModelOptionsModel
                    currentIndex: {
                        const idx = indexOfValue(GroqLlmClient.selectedModel);
                        return idx >= 0 ? idx : 0;
                    }

                    onActivated: index => {
                        GroqLlmClient.selectedModel = currentValue;
                    }
                }
            }

            PreferenceDivider {}

            PreferenceRow {
                title: qsTr("Style Preset")
                description: qsTr("Choose how your voice transcription is formatted")
                layoutVertical: true
                opacity: GroqLlmClient.enabled ? 1.0 : 0.5
                enabled: GroqLlmClient.enabled

                StyledComboBox {
                    id: presetCombo
                    Layout.fillWidth: true
                    textRole: "name"
                    valueRole: "code"
                    model: presetOptionsModel
                    currentIndex: {
                        const idx = indexOfValue(GroqLlmClient.activePreset);
                        return idx >= 0 ? idx : 0;
                    }

                    onActivated: index => {
                        GroqLlmClient.activePreset = currentValue;
                    }
                }
            }
        }

        PreferenceCard {
            title: qsTr("Style Precision / Creativity")
            description: qsTr("Lower values produce exact corrections; higher values produce more creative rewrites")
            opacity: GroqLlmClient.enabled ? 1.0 : 0.5
            enabled: GroqLlmClient.enabled

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
                            text: qsTr("Precision / Creativity")
                            variant: "body"
                            customWeight: Font.Medium
                        }

                        StyledText {
                            text: qsTr("Exact (0.0) — Creative (1.0)")
                            variant: "caption"
                            colorRole: "secondary"
                        }
                    }

                    StyledText {
                        text: GroqLlmClient.temperature.toFixed(2)
                        variant: "body"
                        customWeight: Font.Bold
                        colorRole: "accent"
                        Layout.preferredWidth: 40
                        horizontalAlignment: Text.AlignRight
                    }

                    StyledButton {
                        id: resetTempBtn
                        text: qsTr("Reset")
                        variant: "flat"
                        size: "small"
                        enabled: Math.abs(GroqLlmClient.temperature - 0.1) > 0.01
                        onClicked: {
                            GroqLlmClient.temperature = 0.1;
                        }
                    }
                }

                StyledSlider {
                    id: tempSlider
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    stepSize: 0.05
                    value: GroqLlmClient.temperature
                    onMoved: {
                        GroqLlmClient.temperature = value;
                    }
                }
            }
        }

        PreferenceCard {
            title: GroqLlmClient.activePreset === "custom" ? qsTr("Custom Instructions") : qsTr("Preset Instructions")
            description: GroqLlmClient.activePreset === "custom" ? qsTr(
                                                                       "Define specific formatting and transformation instructions") :
                                                                   qsTr("Standard instructions applied by the selected preset")
            opacity: GroqLlmClient.enabled ? 1.0 : 0.5
            enabled: GroqLlmClient.enabled

            PreferenceRow {
                layoutVertical: true

                RowLayout {
                    Layout.fillWidth: true

                    StyledText {
                        text: GroqLlmClient.activePreset === "custom" ? qsTr("Instructions") : qsTr(
                                                                            "Instructions (Read-only)")
                        variant: "body"
                        customWeight: Font.Medium
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledText {
                        text: GroqLlmClient.activePreset === "custom" ? qsTr("%1 characters").arg(
                                                                            customPromptArea.text.length) : qsTr(
                                                                            "Preset Managed")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    implicitHeight: 100
                    clip: true

                    TextArea {
                        id: customPromptArea
                        readOnly: GroqLlmClient.activePreset !== "custom"
                        text: GroqLlmClient.activePreset === "custom" ? GroqLlmClient.customPrompt :
                                                                        GroqLlmClient.systemPromptForPreset(
                                                                            GroqLlmClient.activePreset)
                        placeholderText: qsTr(
                                             "e.g. Clean up spoken language, fix grammar, and format lists in markdown...")
                        placeholderTextColor: Theme.textPlaceholder
                        selectByMouse: true
                        wrapMode: TextArea.Wrap
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.textPrimary

                        background: Rectangle {
                            color: customPromptArea.readOnly ? Theme.cardBgSubtle : Theme.inputBg
                            border.color: customPromptArea.activeFocus ? Theme.focusRingColor : Theme.inputBorder
                            border.width: customPromptArea.activeFocus ? Theme.focusRingWidth : 1
                            radius: Theme.radiusSm
                        }

                        onTextChanged: {
                            if (activeFocus && GroqLlmClient.activePreset === "custom") {
                                saveLlmPromptTimer.restart();
                            }
                        }
                    }
                }

                Timer {
                    id: saveLlmPromptTimer
                    interval: 350
                    repeat: false
                    onTriggered: {
                        if (GroqLlmClient.activePreset === "custom") {
                            GroqLlmClient.customPrompt = customPromptArea.text.trim();
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    visible: GroqLlmClient.activePreset === "custom"

                    Item {
                        Layout.fillWidth: true
                    }

                    StyledButton {
                        id: clearPromptBtn
                        text: qsTr("Clear")
                        enabled: customPromptArea.text.length > 0
                        size: "small"
                        onClicked: {
                            customPromptArea.text = "";
                            GroqLlmClient.customPrompt = "";
                        }
                    }

                    StyledButton {
                        id: savePromptBtn
                        text: qsTr("Save")
                        size: "small"
                        variant: "primary"
                        onClicked: {
                            saveLlmPromptTimer.stop();
                            GroqLlmClient.customPrompt = customPromptArea.text.trim();
                        }
                    }
                }
            }
        }
    }
}
