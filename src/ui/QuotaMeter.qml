pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTranscribe
import "controls"

StyledCard {
    id: root

    property string meterTitle: ""
    property real usageFraction: 0.0
    property string remainingText: ""
    property string limitText: ""
    property string resetText: ""
    property bool hasData: false
    property bool loading: false
    property real warningThreshold: 0.60
    property real dangerThreshold: 0.85
    property color customAccentColor: Theme.accentColor
    property string customBadgeText: ""

    readonly property string resolvedStatusType: {
        if (!hasData)
        return "neutral";
        if (usageFraction > dangerThreshold)
        return "danger";
        if (usageFraction > warningThreshold)
        return "warning";
        return "success";
    }

    readonly property color progressColor: {
        if (usageFraction > dangerThreshold)
        return Theme.colorDanger;
        if (usageFraction > warningThreshold)
        return Theme.colorWarning;
        return customAccentColor;
    }

    readonly property string badgeText: {
        if (customBadgeText.length > 0)
        return customBadgeText;
        if (loading)
        return qsTr("Checking…");
        if (!hasData)
        return qsTr("Ready");
        return qsTr("%1% used").arg(Math.round(usageFraction * 100));
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSm

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            StyledText {
                text: root.meterTitle
                variant: "subheading"
                visible: root.meterTitle.length > 0
            }

            StyledText {
                text: root.description
                variant: "caption"
                colorRole: "secondary"
                visible: root.description.length > 0
            }
        }

        StateBadge {
            text: root.badgeText
            statusType: root.resolvedStatusType
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs

        StyledText {
            text: root.remainingText
            variant: "display"
        }

        StyledText {
            text: root.limitText
            variant: "caption"
            colorRole: "secondary"
            Layout.alignment: Qt.AlignBaseline
            visible: root.limitText.length > 0
        }
    }

    ProgressBar {
        Layout.fillWidth: true
        implicitHeight: 8
        from: 0.0
        to: 1.0
        value: root.hasData ? Math.min(Math.max(root.usageFraction, 0.0), 1.0) : 0

        background: Rectangle {
            implicitHeight: 8
            radius: Theme.radiusCircle
            color: Theme.controlBg
            border.color: Theme.controlBorder
            border.width: 1
        }

        contentItem: Item {
            implicitHeight: 8
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * (root.hasData ? Math.min(Math.max(root.usageFraction, 0.0), 1.0) : 0)
                radius: Theme.radiusCircle
                color: root.progressColor

                Behavior on width {
                    NumberAnimation {
                        duration: Theme.animNormal
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        visible: root.resetText.length > 0

        StyledText {
            text: root.resetText
            variant: "caption"
            colorRole: "secondary"
        }
    }
}
