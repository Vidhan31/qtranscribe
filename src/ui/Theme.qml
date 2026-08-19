pragma Singleton
import QtQuick
import QTranscribe

QtObject {
    id: root

    property string themeMode: ColorUtils.themeMode
    readonly property bool isDark: ColorUtils.isDark
    readonly property bool systemThemeIsDark: ColorUtils.systemThemeIsDark
    readonly property string systemThemeName: ColorUtils.systemThemeName

    onThemeModeChanged: {
        if (themeMode !== ColorUtils.themeMode) {
            ColorUtils.themeMode = themeMode;
        }
    }

    readonly property Connections _themeModeConn: Connections {
        target: ColorUtils
        function onThemeModeChanged() {
            if (root.themeMode !== ColorUtils.themeMode) {
                root.themeMode = ColorUtils.themeMode;
            }
        }
    }

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 14
    readonly property int spacingLg: 20
    readonly property int spacingXl: 28

    readonly property int sidebarWidth: 220
    readonly property int headerHeight: 48

    readonly property int fontSizeSmall: 10
    readonly property int fontSizeCaption: 11
    readonly property int fontSizeBody: 13
    readonly property int fontSizeSubheading: 15
    readonly property int fontSizeHeading: 18
    readonly property int fontSizeDisplay: 22

    readonly property int radiusNone: 0
    readonly property int radiusXs: 4
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 18
    readonly property int radiusCircle: 9999

    readonly property int animFast: 120
    readonly property int animNormal: 220
    readonly property int animSlow: 350

    readonly property SystemPalette _sysPalette: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    readonly property color systemAccent: (_sysPalette.accent.a > 0.01) ? _sysPalette.accent : _sysPalette.highlight
    readonly property color accentColor: systemAccent
    readonly property color accentColorHover: ColorUtils.tint(accentColor, isDark ? "#26ffffff" : "#1a000000")
    readonly property color accentColorPressed: ColorUtils.tint(accentColor, "#33000000")

    readonly property color colorPrimary: accentColor
    readonly property color colorPrimaryHover: accentColorHover
    readonly property color colorPrimaryPressed: accentColorPressed

    readonly property color textLink: accentColor
    readonly property color focusRingColor: ColorUtils.withAlpha(accentColor, 0.6)
    readonly property int focusRingWidth: 2
    readonly property color sidebarItemSelected: ColorUtils.withAlpha(accentColor, isDark ? 0.22 : 0.16)

    readonly property color windowBg: isDark ? "#1C1C1E" : "#F2F2F7"
    readonly property color sidebarBg: isDark ? "#141416" : "#EBEBF0"
    readonly property color sidebarBorder: isDark ? "#2C2C2E" : "#D1D1D6"
    readonly property color headerBg: isDark ? "#18181A" : "#F2F2F7"
    readonly property color headerBorder: isDark ? "#2C2C2E" : "#D1D1D6"

    readonly property color cardBg: isDark ? "#242426" : "#FFFFFF"
    readonly property color cardBgDefault: cardBg
    readonly property color cardBgElevated: isDark ? "#2C2C2E" : "#FFFFFF"
    readonly property color cardBgSubtle: isDark ? "#1E1E20" : "#F8F8FA"
    readonly property color cardBorder: isDark ? "#14ffffff" : "#E5E5EA"
    readonly property color cardBorderHover: isDark ? "#29ffffff" : "#C7C7CC"

    readonly property color controlBg: isDark ? "#2C2C2E" : "#E5E5EA"
    readonly property color controlBgHover: isDark ? "#3A3A3C" : "#DCDCE2"
    readonly property color controlBgPressed: isDark ? "#222224" : "#D1D1D6"
    readonly property color controlBorder: isDark ? "#1fffffff" : "#D1D1D6"

    readonly property color inputBg: isDark ? "#18181A" : "#FFFFFF"
    readonly property color inputBorder: isDark ? "#1fffffff" : "#D1D1D6"
    readonly property color inputBorderFocus: accentColor

    readonly property color hoverOverlay: isDark ? "#0fffffff" : "#0a000000"
    readonly property color pressedOverlay: isDark ? "#33000000" : "#14000000"
    readonly property color selectedBg: ColorUtils.withAlpha(accentColor, isDark ? 0.18 : 0.14)
    readonly property color sidebarItemHover: isDark ? "#0dffffff" : "#0a000000"

    readonly property color buttonPrimaryBg: accentColor
    readonly property color buttonPrimaryBgHover: accentColorHover
    readonly property color buttonPrimaryBgPressed: accentColorPressed

    readonly property color buttonDangerBg: colorDanger
    readonly property color buttonDangerBgHover: ColorUtils.tint(colorDanger, isDark ? "#1affffff" : "#14000000")
    readonly property color buttonDangerBgPressed: colorDangerHover

    readonly property color sliderThumbBg: "#FFFFFF"
    readonly property color sliderThumbBgPressed: isDark ? "#E5E5EA" : "#EBEBF0"
    readonly property color sliderThumbBorder: isDark ? "#48484A" : "#C7C7CC"

    readonly property color textPrimary: isDark ? "#FFFFFF" : "#1C1C1E"
    readonly property color textSecondary: isDark ? "#98989D" : "#6C6C70"
    readonly property color textTertiary: isDark ? "#636366" : "#8E8E93"
    readonly property color textPlaceholder: isDark ? "#48484A" : "#AEAEC2"
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color textOnDanger: "#FFFFFF"

    readonly property color colorSuccess: isDark ? "#30D158" : "#34C759"
    readonly property color colorWarning: isDark ? "#FF9F0A" : "#FF9500"
    readonly property color colorDanger: isDark ? "#FF453A" : "#FF3B30"
    readonly property color colorDangerHover: isDark ? "#D7382E" : "#E02B20"
    readonly property color colorPurple: isDark ? "#BF5AF2" : "#AF52DE"
    readonly property color colorTeal: isDark ? "#64D2FF" : "#30B0C7"

    readonly property int switchTrackWidth: 42
    readonly property int switchTrackHeight: 24
    readonly property int switchThumbSize: 20
    readonly property color switchTrackOff: isDark ? "#39393D" : "#E5E5EA"
    readonly property color switchTrackOn: accentColor
    readonly property color switchThumb: "#FFFFFF"
    function withAlpha(c: color, alpha: real): color {
        return ColorUtils.withAlpha(c, alpha);
    }

    function tint(baseCol: color, tintCol: color): color {
        return ColorUtils.tint(baseCol, tintCol);
    }

    function statusBgColor(c: color, alpha: real): color {
        return ColorUtils.statusBg(c, isDark ? alpha : alpha * 0.85);
    }

    function statusBorderColor(c: color, alpha: real): color {
        return ColorUtils.statusBorder(c, isDark ? alpha : alpha * 0.75);
    }
}
