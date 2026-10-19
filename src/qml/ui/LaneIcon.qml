import QtQuick
import QtQuick.Window

// Shared theme-icon shim: resolves a freedesktop icon name through the
// laneicon image provider (C++ QIcon::fromTheme backend). Replaces the
// private IconImage API FormCard's delegates used.
Image {
    id: root

    // Icon name to look up (e.g. "application-x-executable").
    property string iconName: ""

    source: iconName !== "" ? "image://laneicon/" + iconName : ""
    // Fixed physical source size: binding sourceSize to width/height is
    // self-referential (sourceSize feeds implicitWidth/Height which feed
    // width/height) and RangeErrors at runtime on macOS. 64px covers the
    // largest use (Settings rows, buttons) at 2x Retina.
    sourceSize: Qt.size(64, 64)
    fillMode: Image.PreserveAspectFit
    // bundle: icons go through QFileIconProvider, whose macOS backend talks
    // to AppKit. That has to stay on the GUI thread. Theme icons stay async.
    asynchronous: !iconName.startsWith("bundle:")
    // True once a theme icon actually loaded; callers that want a
    // collapsing slot (button rows) bind visible to this.
    readonly property bool loaded: iconName !== "" && status === Image.Ready
}
