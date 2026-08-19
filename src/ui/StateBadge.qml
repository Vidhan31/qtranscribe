pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QTranscribe
import "controls"

Rectangle {
    id: root

    property string text: ""
    property string statusType: "neutral"
    property color customColor: "transparent"
    property bool showDot: false
    property bool pulsing: false
    property int pixelSize: Theme.fontSizeCaption

    visible: text.length > 0 || showDot

    implicitWidth: badgeRow.implicitWidth + (Theme.spacingSm * 2)
    implicitHeight: Math.max(22, badgeRow.implicitHeight + (Theme.spacingXs * 2))
    radius: Theme.radiusSm

    readonly property color resolvedColor: {
        if (customColor.a > 0.001) {
            return customColor;
        }
        if (statusType === "success") {
            return Theme.colorSuccess;
        }
        if (statusType === "warning") {
            return Theme.colorWarning;
        }
        if (statusType === "danger") {
            return Theme.colorDanger;
        }
        if (statusType === "accent") {
            return Theme.accentColor;
        }
        return Theme.withAlpha(Theme.textPrimary, 0.85);
    }

    color: Theme.statusBgColor(resolvedColor, 0.15)
    border.color: Theme.statusBorderColor(resolvedColor, 0.4)
    border.width: 1

    RowLayout {
        id: badgeRow
        anchors.centerIn: parent
        spacing: Theme.spacingXs

        Rectangle {
            id: indicatorDot
            color: root.resolvedColor
            radius: 3
            visible: root.showDot
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6

            SequentialAnimation on opacity {
                running: root.pulsing && root.visible
                loops: Animation.Infinite
                OpacityAnimator {
                    to: 0.3
                    duration: 400
                }
                OpacityAnimator {
                    to: 1.0
                    duration: 400
                }
            }
        }

        StyledText {
            id: badgeLabel
            text: root.text
            customPixelSize: root.pixelSize
            customWeight: Font.DemiBold
            customColor: root.resolvedColor
            visible: root.text.length > 0
        }
    }
}
