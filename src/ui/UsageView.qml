pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

Item {
    id: root

    signal navigateRequested(var target)

    implicitWidth: 620
    implicitHeight: 540

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingMd

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingXs
                Layout.bottomMargin: Theme.spacingSm
                spacing: Theme.spacingMd

                ColumnLayout {
                    spacing: 4

                    StyledText {
                        text: qsTr("Activity")
                        variant: "heading"
                    }

                    StyledText {
                        text: qsTr("Daily quotas and session statistics")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                StyledButton {
                    id: refreshBtn
                    text: GroqUsageTracker.checkingQuota ? qsTr("Refreshing…") : qsTr("Refresh")
                    enabled: GroqApiClient.apiKeySet && !GroqUsageTracker.checkingQuota
                    size: "small"
                    onClicked: GroqUsageTracker.refreshQuota()
                }

                StyledButton {
                    id: clearSessionBtn
                    text: qsTr("Clear Session")
                    enabled: GroqUsageTracker.sessionTotalRequests > 0
                    size: "small"
                    onClicked: GroqUsageTracker.resetSessionStats()
                }
            }

            StatusBanner {
                visible: GroqUsageTracker.isRateLimited
                bannerType: "danger"
                title: qsTr("Rate Limit Exceeded")
                message: GroqUsageTracker.retryAfterSeconds > 0 ? qsTr(
                                                                      "Rate limit reached. Please wait %1 seconds before trying again.").arg(
                                                                      GroqUsageTracker.retryAfterSeconds) : qsTr(
                                                                      "Rate limit reached. Requests will resume once your quota resets.")
            }

            StatusBanner {
                visible: GroqUsageTracker.quotaCheckError.length > 0
                bannerType: "warning"
                title: qsTr("Quota Check Warning")
                message: GroqUsageTracker.quotaCheckError
            }

            StatusBanner {
                visible: !GroqApiClient.apiKeySet
                bannerType: "info"
                title: qsTr("Groq API Key Not Configured")
                message: qsTr(
                             "Enter your Groq API key in Settings to track live rate limits, tokens, and transcription performance.")
                actionText: qsTr("Open Settings")
                onActionClicked: root.navigateRequested("apiKey")
            }

            StatusBanner {
                visible: GroqApiClient.apiKeySet && !GroqUsageTracker.hasData
                bannerType: "info"
                title: qsTr("Ready to Query Groq Quotas")
                message: qsTr(
                             "Rate limit metrics update automatically on dictations, or click 'Check Quota' to fetch real-time limits now.")
                actionText: GroqUsageTracker.checkingQuota ? qsTr("Fetching…") : qsTr("Fetch Limits Now")
                actionEnabled: !GroqUsageTracker.checkingQuota
                onActionClicked: GroqUsageTracker.refreshQuota()
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.width >= 560 ? 2 : 1
                columnSpacing: Theme.spacingMd
                rowSpacing: Theme.spacingMd
                visible: GroqApiClient.apiKeySet

                QuotaMeter {
                    meterTitle: qsTr("Daily Requests")
                    description: qsTr("Daily request allowance")
                    hasData: GroqUsageTracker.hasData
                    loading: GroqUsageTracker.checkingQuota
                    usageFraction: GroqUsageTracker.requestsUsageFraction
                    remainingText: GroqUsageTracker.hasData ? qsTr("%1").arg(GroqUsageTracker.remainingRequests) : (
                                                                  GroqUsageTracker.checkingQuota ? qsTr("Loading…") :
                                                                                                   "—")
                    limitText: GroqUsageTracker.hasData ? qsTr("remaining of %1").arg(GroqUsageTracker.limitRequests) :
                                                          qsTr("No request data yet")
                    resetText: GroqUsageTracker.resetRequests.length > 0 ? qsTr("Resets in: %1").arg(
                                                                               GroqUsageTracker.resetRequests) : qsTr(
                                                                               "Reset window: Rolling 24-hour")
                }

                QuotaMeter {
                    meterTitle: qsTr("Minute Rate Limit")
                    description: qsTr("Per-minute token capacity")
                    hasData: GroqUsageTracker.hasData
                    loading: GroqUsageTracker.checkingQuota
                    usageFraction: GroqUsageTracker.tokensUsageFraction
                    remainingText: GroqUsageTracker.hasData ? qsTr("%1").arg(GroqUsageTracker.remainingTokens) : (
                                                                  GroqUsageTracker.checkingQuota ? qsTr("Loading…") :
                                                                                                   "—")
                    limitText: GroqUsageTracker.hasData ? qsTr("remaining of %1").arg(GroqUsageTracker.limitTokens) :
                                                          qsTr("No token data yet")
                    resetText: GroqUsageTracker.resetTokens.length > 0 ? qsTr("Resets in: %1").arg(
                                                                             GroqUsageTracker.resetTokens) : qsTr(
                                                                             "Reset window: Rolling 60-second")
                }
            }

            StyledCard {
                Layout.fillWidth: true
                visible: GroqApiClient.apiKeySet && GroqUsageTracker.hasAudioSecondsLimit

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    StyledText {
                        text: qsTr("Audio Quota")
                        variant: "subheading"
                    }

                    StyledText {
                        text: qsTr("Audio seconds limit for speech processing")
                        variant: "caption"
                        colorRole: "secondary"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs

                    StyledText {
                        text: qsTr("%1s").arg(GroqUsageTracker.remainingAudioSeconds)
                        variant: "display"
                        colorRole: "accent"
                    }

                    StyledText {
                        text: qsTr("remaining of %1s").arg(GroqUsageTracker.limitAudioSeconds)
                        variant: "caption"
                        colorRole: "secondary"
                        Layout.alignment: Qt.AlignBaseline
                    }
                }
            }

            StyledCard {
                Layout.fillWidth: true

                StyledText {
                    text: qsTr("Session Statistics")
                    variant: "subheading"
                    Layout.topMargin: Theme.spacingXs
                    Layout.bottomMargin: Theme.spacingXs
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width >= 560 ? 3 : 2
                    columnSpacing: Theme.spacingMd
                    rowSpacing: Theme.spacingSm

                    MetricCard {
                        title: qsTr("Total Requests")
                        value: qsTr("%1").arg(GroqUsageTracker.sessionTotalRequests)
                    }

                    MetricCard {
                        title: qsTr("Transcriptions")
                        value: qsTr("%1").arg(GroqUsageTracker.sessionSttRequests)
                        valueColor: Theme.accentColor
                    }

                    MetricCard {
                        title: qsTr("Text Enhancements")
                        value: qsTr("%1").arg(GroqUsageTracker.sessionLlmRequests)
                        valueColor: Theme.colorSuccess
                    }

                    MetricCard {
                        title: qsTr("Input Tokens")
                        value: qsTr("%1").arg(GroqUsageTracker.sessionPromptTokens)
                    }

                    MetricCard {
                        title: qsTr("Output Tokens")
                        value: qsTr("%1").arg(GroqUsageTracker.sessionCompletionTokens)
                    }

                    MetricCard {
                        title: qsTr("Total Tokens")
                        value: qsTr("%1").arg(GroqUsageTracker.sessionTotalTokens)
                        valueColor: Theme.accentColor
                    }
                }
            }

            StyledCard {
                Layout.fillWidth: true

                StyledText {
                    text: qsTr("Last Request")
                    variant: "subheading"
                    Layout.topMargin: Theme.spacingXs
                    Layout.bottomMargin: Theme.spacingXs
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    visible: GroqUsageTracker.lastUpdatedTimestamp.length > 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        MetricCard {
                            Layout.fillWidth: true
                            title: qsTr("Task")
                            value: GroqUsageTracker.lastEndpoint
                            valuePixelSize: Theme.fontSizeBody
                        }

                        MetricCard {
                            Layout.fillWidth: true
                            title: qsTr("Model")
                            value: GroqUsageTracker.lastModel.length > 0 ? GroqUsageTracker.lastModel : qsTr("Default")
                            valuePixelSize: Theme.fontSizeBody
                        }

                        ColumnLayout {
                            spacing: 2

                            StyledText {
                                text: qsTr("Status")
                                variant: "caption"
                                colorRole: "secondary"
                            }

                            StateBadge {
                                text: qsTr("HTTP %1").arg(GroqUsageTracker.lastHttpStatus)
                                statusType: GroqUsageTracker.lastHttpStatus === 200 ? "success" : "danger"
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        MetricCard {
                            Layout.fillWidth: true
                            title: qsTr("Latency")
                            value: qsTr("%1 ms").arg(GroqUsageTracker.lastLatencyMs)
                            valuePixelSize: Theme.fontSizeBody
                        }

                        MetricCard {
                            Layout.fillWidth: true
                            title: qsTr("Time")
                            value: GroqUsageTracker.lastUpdatedTimestamp
                            valuePixelSize: Theme.fontSizeBody
                        }
                    }
                }

                StyledText {
                    visible: GroqUsageTracker.lastUpdatedTimestamp.length === 0
                    text: qsTr("No requests recorded in this session yet.")
                    variant: "body"
                    colorRole: "secondary"
                }
            }

            StyledText {
                Layout.fillWidth: true
                text: qsTr("Groq rate limits apply at the organization tier level across all active API keys.")
                variant: "caption"
                colorRole: "tertiary"
            }
        }
    }
}
