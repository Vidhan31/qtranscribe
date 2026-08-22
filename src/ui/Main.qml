pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform
import QTranscribe
import "controls"
import "settings"

ApplicationWindow {
    id: root
    width: 960
    height: 660
    minimumWidth: 760
    minimumHeight: 520
    visible: false
    title: qsTr("QTranscribe")
    color: Theme.windowBg

    property int activeNavIndex: 0

    function navigateToSection(target: var) {
        if (typeof target === "number") {
            if (target >= 0 && target < navModel.count) {
                root.activeNavIndex = target;
            }
            return;
        }
        if (typeof target === "string") {
            for (let i = 0; i < navModel.count; ++i) {
                if (navModel.get(i).sectionId === target) {
                    root.activeNavIndex = i;
                    return;
                }
            }
        }
    }

    onClosing: close => {
        close.accepted = false;
        root.hide();
    }

    Platform.SystemTrayIcon {
        id: trayIcon
        visible: true
        icon.name: SpeechController.recording ? TrayIconHelper.trayIconRecordingName : TrayIconHelper.trayIconName
        icon.source: SpeechController.recording ? TrayIconHelper.trayIconRecordingPath(Theme.isDark) :
                                                  TrayIconHelper.trayIconPath(Theme.isDark)
        tooltip: qsTr("QTranscribe")

        menu: Platform.Menu {
            Platform.MenuItem {
                text: qsTr("Open QTranscribe")
                onTriggered: {
                    root.show();
                    root.raise();
                    root.requestActivate();
                }
            }
            Platform.MenuItem {
                text: qsTr("Quit")
                onTriggered: Qt.quit()
            }
        }

        onActivated: activationReason => {
            if (activationReason === Platform.SystemTrayIcon.Trigger || activationReason
                    === Platform.SystemTrayIcon.DoubleClick) {
                root.show();
                root.raise();
                root.requestActivate();
            }
        }
    }

    ClipboardWarningDialog {
        id: clipboardWarningDialog
        onAccepted: {
            SpeechController.startRecording();
        }
    }

    Connections {
        target: SpeechController
        function onRequestShowWindow() {
            root.show();
            root.raise();
            root.requestActivate();
        }
        function onRequestQuitApp() {
            Qt.quit();
        }
        function onRequestClipboardWarningModal() {
            root.show();
            root.raise();
            root.requestActivate();
            clipboardWarningDialog.open();
        }
    }

    ListModel {
        id: navModel

        ListElement {
            section: "MAIN"
            title: "Dictate"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/mic.svg"
            navIndex: 0
            sectionId: "dictate"
        }
        ListElement {
            section: "MAIN"
            title: "History"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/history.svg"
            navIndex: 1
            sectionId: "history"
        }
        ListElement {
            section: "MAIN"
            title: "Activity"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/activity.svg"
            navIndex: 2
            sectionId: "activity"
        }

        ListElement {
            section: "PREFERENCES"
            title: "API Key"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/key.svg"
            navIndex: 3
            sectionId: "apiKey"
        }
        ListElement {
            section: "PREFERENCES"
            title: "Dictation"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/speech.svg"
            navIndex: 4
            sectionId: "dictation"
        }
        ListElement {
            section: "PREFERENCES"
            title: "Offline Dictation"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/bolt.svg"
            navIndex: 5
            sectionId: "offline"
        }
        ListElement {
            section: "PREFERENCES"
            title: "Text Enhancement"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/sparkles.svg"
            navIndex: 6
            sectionId: "enhancement"
        }
        ListElement {
            section: "PREFERENCES"
            title: "System & Typing"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/keyboard.svg"
            navIndex: 7
            sectionId: "system"
        }

        ListElement {
            section: "INFO"
            title: "About"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/info.svg"
            navIndex: 8
            sectionId: "about"
        }
        ListElement {
            section: "INFO"
            title: "License"
            iconSource: "qrc:/qt/qml/QTranscribe/assets/icons/license.svg"
            navIndex: 9
            sectionId: "license"
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.fillHeight: true
            color: Theme.sidebarBg

            StyledDivider {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                orientation: Qt.Vertical
                dividerColor: Theme.sidebarBorder
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                spacing: Theme.spacingSm

                ListView {
                    id: navListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.spacingXs
                    clip: true
                    spacing: 2
                    model: navModel

                    delegate: Item {
                        id: navDelegate
                        required property string section
                        required property string title
                        required property string iconSource
                        required property int navIndex
                        required property int index

                        readonly property bool isSelected: root.activeNavIndex === navDelegate.navIndex
                        readonly property bool isFirstInSection: navDelegate.index === 0 || navModel.get(
                                                                     navDelegate.index - 1).section
                                                                 !== navDelegate.section

                        width: navListView.width
                        implicitHeight: (navDelegate.isFirstInSection ? 32 : 0) + 34

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2

                            StyledText {
                                text: navDelegate.section
                                variant: "small"
                                customWeight: Font.Bold
                                colorRole: "tertiary"
                                visible: navDelegate.isFirstInSection
                                Layout.leftMargin: Theme.spacingSm
                                Layout.topMargin: navDelegate.index === 0 ? 4 : Theme.spacingMd
                                Layout.bottomMargin: 4
                            }

                            Rectangle {
                                id: itemPill
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                radius: Theme.radiusSm
                                color: {
                                    if (navDelegate.isSelected)
                                    return Theme.sidebarItemSelected;
                                    if (navMouse.containsMouse)
                                    return Theme.sidebarItemHover;
                                    return "transparent";
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingSm + 2
                                    anchors.rightMargin: Theme.spacingSm
                                    spacing: Theme.spacingSm

                                    StyledIcon {
                                        source: navDelegate.iconSource
                                        size: 16
                                        color: navDelegate.isSelected ? Theme.accentColor : (navMouse.containsMouse
                                                                                             ? Theme.textPrimary :
                                                                                               Theme.textSecondary)
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    StyledText {
                                        text: navDelegate.title
                                        variant: "body"
                                        customWeight: navDelegate.isSelected ? Font.DemiBold : Font.Normal
                                        customColor: navDelegate.isSelected ? Theme.accentColor : (
                                                                                  navMouse.containsMouse
                                                                                  ? Theme.textPrimary :
                                                                                    Theme.textSecondary)
                                        verticalAlignment: Text.AlignVCenter
                                        Layout.alignment: Qt.AlignVCenter
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: navMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.navigateToSection(navDelegate.navIndex);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackLayout {
                id: viewStack
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                currentIndex: root.activeNavIndex

                onCurrentIndexChanged: {
                    viewTransitionAnim.restart();
                }

                SpeechPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onNavigateRequested: target => {
                        root.navigateToSection(target);
                    }
                }

                HistoryView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                UsageView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onNavigateRequested: target => {
                        root.navigateToSection(target);
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    ApiSettingsPage {
                        width: parent.width
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    SpeechSettingsPage {
                        width: parent.width
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    OfflineSettingsPage {
                        width: parent.width
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    LlmSettingsPage {
                        width: parent.width
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    SystemSettingsPage {
                        width: parent.width
                    }
                }

                CreditsView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                LicenseView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            SequentialAnimation {
                id: viewTransitionAnim

                ParallelAnimation {
                    NumberAnimation {
                        target: viewStack
                        property: "opacity"
                        from: 0.4
                        to: 1.0
                        duration: Theme.animNormal
                        easing.type: Easing.OutCubic
                    }

                    NumberAnimation {
                        target: viewStack
                        property: "y"
                        from: 4
                        to: 0
                        duration: Theme.animNormal
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }
}
