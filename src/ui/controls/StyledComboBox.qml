pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QTranscribe

ComboBox {
    id: root

    implicitWidth: 200
    implicitHeight: 36
    font.pixelSize: Theme.fontSizeBody

    contentItem: Text {
        leftPadding: Theme.spacingMd
        rightPadding: root.indicator.width + Theme.spacingMd
        text: root.displayText
        font.pixelSize: root.font.pixelSize
        font.weight: root.font.weight
        color: root.enabled ? Theme.textPrimary : Theme.textPlaceholder
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Item {
        x: root.width - width - Theme.spacingMd
        y: (root.height - height) / 2
        width: 12
        height: 12

        Text {
            anchors.centerIn: parent
            text: "▾"
            font.pixelSize: 12
            color: root.popup.visible ? Theme.accentColor : Theme.textSecondary
            rotation: root.popup.visible ? 180 : 0

            Behavior on rotation {
                NumberAnimation {
                    duration: Theme.animFast
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    background: Rectangle {
        implicitWidth: 160
        implicitHeight: 36
        color: root.down ? Theme.controlBgPressed : (root.hovered ? Theme.controlBgHover : Theme.controlBg)
        border.color: (root.activeFocus || root.visualFocus) ? Theme.focusRingColor : Theme.controlBorder
        border.width: (root.activeFocus || root.visualFocus) ? Theme.focusRingWidth : 1
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
                easing.type: Easing.OutCubic
            }
        }
    }

    popup: Popup {
        y: root.height + 4
        width: Math.max(root.width, 180)
        implicitHeight: Math.min(260, popupListView.contentHeight + topPadding + bottomPadding)
        padding: 4

        background: Rectangle {
            color: Theme.cardBgElevated
            border.color: Theme.cardBorder
            border.width: 1
            radius: Theme.radiusMd
        }

        contentItem: ListView {
            id: popupListView
            clip: true
            implicitHeight: contentHeight
            model: root.delegateModel
            currentIndex: root.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: popupListView.contentHeight > popupListView.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                width: 6
            }
        }
    }

    delegate: ItemDelegate {
        id: delegateItem
        required property int index
        required property var model
        required property var modelData

        readonly property string itemText: {
            if (root.textRole && root.textRole.length > 0) {
                if (delegateItem.model && delegateItem.model[root.textRole] !== undefined) {
                    return String(delegateItem.model[root.textRole]);
                }
                if (delegateItem.modelData && delegateItem.modelData[root.textRole] !== undefined) {
                    return String(delegateItem.modelData[root.textRole]);
                }
            }
            if (delegateItem.modelData !== undefined && delegateItem.modelData !== null) {
                return String(delegateItem.modelData);
            }
            return root.textAt(delegateItem.index);
        }

        width: ListView.view ? ListView.view.width : root.width
        implicitHeight: 32
        highlighted: root.highlightedIndex === delegateItem.index
        hoverEnabled: true

        onClicked: {
            root.currentIndex = delegateItem.index;
            root.activated(delegateItem.index);
            root.popup.close();
        }

        contentItem: Text {
            text: delegateItem.itemText
            font.pixelSize: root.font.pixelSize
            font.weight: delegateItem.highlighted ? Font.DemiBold : Font.Normal
            color: delegateItem.highlighted ? Theme.accentColor : Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            leftPadding: Theme.spacingSm
            rightPadding: Theme.spacingSm
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: {
                if (delegateItem.highlighted)
                return Theme.sidebarItemSelected;
                if (delegateItem.hovered || delegateItem.visualFocus)
                return Theme.sidebarItemHover;
                return "transparent";
            }
        }
    }
}
