import QtQuick

// Shared theme-icon shim: resolves a freedesktop icon name through the
// laneicon image provider (C++ QIcon::fromTheme backend). Replaces the
// private IconImage API FormCard's delegates used.
Image {
    id: root

    // Icon name to look up (e.g. "application-x-executable").
    property string iconName: ""

    source: iconName !== "" ? "image://laneicon/" + iconName : ""
    sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    asynchronous: true
}
