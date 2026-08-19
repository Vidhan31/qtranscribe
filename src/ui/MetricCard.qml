pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QTranscribe
import "controls"

ColumnLayout {
    id: root

    property string title: ""
    property string value: ""
    property string subtitle: ""
    property color valueColor: Theme.textPrimary
    property int valuePixelSize: Theme.fontSizeHeading
    property bool isBold: true

    default property alias extraContent: customContent.data

    spacing: 2

    StyledText {
        text: root.title
        variant: "caption"
        colorRole: "secondary"
        visible: root.title.length > 0
    }

    StyledText {
        text: root.value
        customPixelSize: root.valuePixelSize
        customWeight: root.isBold ? Font.Bold : Font.Normal
        customColor: root.valueColor
        visible: root.value.length > 0
    }

    StyledText {
        text: root.subtitle
        variant: "small"
        colorRole: "tertiary"
        visible: root.subtitle.length > 0
    }

    ColumnLayout {
        id: customContent
        Layout.fillWidth: true
        spacing: 0
    }
}
