pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QTranscribe
import "controls"

Rectangle {
    id: root

    property string title: ""
    property string message: ""
    property string bannerType: "info"
    property color customBorderColor: "transparent"

    property string actionText: ""
    property bool actionHighlighted: true
    property bool actionEnabled: true

    property string secondaryActionText: ""
    property bool secondaryActionHighlighted: false
    property bool secondaryActionEnabled: true

    default property alias extraContent: customContentContainer.data

    signal actionClicked
    signal secondaryActionClicked

    readonly property color resolvedAccentColor: {
        if (bannerType === "warning")
        return Theme.colorWarning;
        if (bannerType === "danger")
        return Theme.colorDanger;
        if (bannerType === "success")
        return Theme.colorSuccess;
        return Theme.accentColor;
    }

    implicitHeight: bannerLayout.implicitHeight + (Theme.spacingMd * 2)
    color: Theme.statusBgColor(resolvedAccentColor, 0.12)
    border.color: customBorderColor.a > 0.001 ? customBorderColor : Theme.statusBorderColor(resolvedAccentColor, 0.4)
    border.width: 1
    radius: 0

    Layout.fillWidth: true

    RowLayout {
        id: bannerLayout
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: Theme.spacingMd
        }
        spacing: Theme.spacingSm

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            StyledText {
                text: root.title
                variant: "body"
                customWeight: Font.DemiBold
                customColor: root.resolvedAccentColor
                visible: root.title.length > 0
            }

            StyledText {
                text: root.message
                variant: "caption"
                colorRole: "secondary"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: root.message.length > 0
            }

            ColumnLayout {
                id: customContentContainer
                Layout.fillWidth: true
                spacing: Theme.spacingXs
            }
        }

        RowLayout {
            spacing: Theme.spacingSm
            visible: root.actionText.length > 0 || root.secondaryActionText.length > 0

            StyledButton {
                id: bannerPrimaryBtn
                text: root.actionText
                enabled: root.actionEnabled
                size: "small"
                variant: root.actionHighlighted ? "primary" : "secondary"
                customAccentColor: root.resolvedAccentColor
                visible: root.actionText.length > 0
                onClicked: root.actionClicked()
            }

            StyledButton {
                id: bannerSecondaryBtn
                text: root.secondaryActionText
                enabled: root.secondaryActionEnabled
                size: "small"
                variant: root.secondaryActionHighlighted ? "primary" : "secondary"
                visible: root.secondaryActionText.length > 0
                onClicked: root.secondaryActionClicked()
            }
        }
    }
}
