pragma Singleton
import QtQuick

// Metrics and semantic colors for the app.lane.ui module. No window or
// palette access here: a singleton cannot see either, so call sites use
// their attached `palette` for theme colors and only come here for
// spacing/size constants and the semantic text colors QPalette lacks.
QtObject {
    id: root

    // Font-relative spacing metric (gridUnit-style), matches
    // Plasma's default 8px form grid.
    readonly property int gridUnit: 8
    readonly property int smallSpacing: Math.round(gridUnit / 4)
    readonly property int largeSpacing: gridUnit
    readonly property int verticalPadding: 10
    readonly property int horizontalPadding: 14

    // Card metrics matching the old FormCard look.
    readonly property real cardWidth: gridUnit * 28
    readonly property real cornerRadius: 6

    // Breeze semantic approximations; these are static because
    // QPalette offers no semantic text roles.
    readonly property color positiveTextColor: "#1ac21c"
    readonly property color negativeTextColor: "#db241c"
    readonly property color neutralTextColor: "#e8a72a"
    readonly property color disabledTextColor: Qt.rgba(0.5, 0.5, 0.5, 1)

    readonly property int shortDuration: 150
    readonly property int longDuration: 250

    // Icon sizes matching the Kirigami.Units sizes we used.
    readonly property int iconSizeSmallMedium: 22
    readonly property int iconSizeSmall: 16
    readonly property int iconSizeMedium: 32
}
