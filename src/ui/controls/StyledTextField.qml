pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QTranscribe

TextField {
    id: root

    property bool isSearchPill: false

    implicitHeight: 34
    placeholderTextColor: Theme.textPlaceholder
    color: Theme.textPrimary
    selectionColor: Theme.accentColor
    selectedTextColor: Theme.textOnAccent
    selectByMouse: true
    font.pixelSize: Theme.fontSizeBody

    leftPadding: isSearchPill ? Theme.spacingMd : Theme.spacingSm + 2
    rightPadding: isSearchPill ? Theme.spacingMd : Theme.spacingSm + 2
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: Text.AlignVCenter

    background: Rectangle {
        color: Theme.inputBg
        border.color: root.activeFocus ? Theme.focusRingColor : Theme.inputBorder
        border.width: root.activeFocus ? Theme.focusRingWidth : 1
        radius: root.isSearchPill ? Theme.radiusCircle : Theme.radiusSm

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.animFast
                easing.type: Easing.OutCubic
            }
        }
    }
}
