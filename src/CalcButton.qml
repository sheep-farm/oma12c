import QtQuick
import QtQuick.Layouts

// HP-12C-style keypad button with the muted, Omarchy-aware palette of
// Omacalc.  Each key carries its primary label plus small gold (f) and
// blue (g) legends.  When a prefix is active the main label switches to
// the corresponding shifted function.
Rectangle {
    id: control

    property string label
    property string fLabel: ""
    property string gLabel: ""

    property string keyValue: label
    property string fKey: ""
    property string gKey: ""

    property string kind: "white"
    property color pageColor: "#101010"
    property color inkColor: "#eeeeee"
    property color accentColor: "#5584aa"
    property string activeKey: ""
    property string prefix: ""

    signal activated()

    readonly property bool keyActive: activeKey !== "" && currentKey === activeKey

    Accessible.role: Accessible.Button
    Accessible.name: control.label
    Accessible.onPressAction: activated()

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }

    function desaturate(color, amount) {
        const gray = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
        return Qt.rgba(
            color.r + (gray - color.r) * amount,
            color.g + (gray - color.g) * amount,
            color.b + (gray - color.b) * amount, 1);
    }

    readonly property bool buttonPressed: hitArea.pressed || keyActive
    readonly property bool buttonHovered: hitArea.containsMouse && !hitArea.pressed

    readonly property color baseColor: {
        if (kind === "gold")
            return mixColors(pageColor, desaturate(Qt.rgba(0.83, 0.63, 0.09, 1), 0.45), 0.55);
        if (kind === "blue")
            return mixColors(pageColor, desaturate(Qt.rgba(0.18, 0.37, 0.55, 1), 0.45), 0.55);
        if (kind === "black")
            return mixColors(pageColor, inkColor, 0.22);
        if (kind === "red")
            return mixColors(pageColor, desaturate(Qt.rgba(0.69, 0.19, 0.19, 1), 0.45), 0.55);
        if (kind === "orange")
            return mixColors(pageColor, desaturate(accentColor, 0.25), 0.45);
        if (kind === "enter")
            return mixColors(pageColor, inkColor, 0.18);
        return mixColors(pageColor, inkColor, 0.06);
    }

    readonly property color textColor: {
        if (kind === "black" || kind === "enter")
            return pageColor;
        if (kind === "gold" || kind === "blue" || kind === "red" || kind === "orange")
            return inkColor;
        return inkColor;
    }

    readonly property color goldColor: mixColors(pageColor, desaturate(Qt.rgba(0.93, 0.72, 0.18, 1), 0.15), 0.92);
    readonly property color blueColor: mixColors(pageColor, desaturate(Qt.rgba(0.32, 0.58, 0.86, 1), 0.15), 0.92);
    readonly property color activeTextColor: {
        if (prefix === "f") return goldColor;
        if (prefix === "g") return blueColor;
        return textColor;
    }

    color: buttonPressed
        ? mixColors(baseColor, inkColor, 0.22)
        : (buttonHovered ? mixColors(baseColor, inkColor, 0.09) : baseColor)
    border.width: kind === "white" ? 1 : 0
    border.color: mixColors(pageColor, inkColor, 0.13)
    radius: 0

    scale: buttonPressed ? 0.97 : 1.0
    transformOrigin: Item.Center
    Behavior on color { ColorAnimation { duration: 80 } }
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }

    readonly property string displayLabel: {
        if (prefix === "f" && fLabel !== "") return fLabel;
        if (prefix === "g" && gLabel !== "") return gLabel;
        return label;
    }
    readonly property string currentKey: {
        if (prefix === "f" && fKey !== "") return fKey;
        if (prefix === "g" && gKey !== "") return gKey;
        return keyValue;
    }

    readonly property real labelOpacity: (prefix === "f" && fLabel === "") || (prefix === "g" && gLabel === "") ? 0.35 : 1.0

    // Gold f legend, top (visible only when no prefix is active).
    Text {
        visible: control.fLabel !== "" && control.prefix === ""
        anchors.top: parent.top
        anchors.topMargin: parent.height * 0.08
        anchors.horizontalCenter: parent.horizontalCenter
        text: control.fLabel
        color: control.goldColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.round(Math.min(parent.height * 0.18, parent.width * 0.13))
        opacity: 0.95
    }

    // Main label.
    Text {
        anchors.centerIn: parent
        text: control.displayLabel
        color: control.activeTextColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.round(Math.min(parent.height * 0.32, parent.width * 0.24))
        opacity: control.labelOpacity
    }

    // Blue g legend, bottom (visible only when no prefix is active).
    Text {
        visible: control.gLabel !== "" && control.prefix === ""
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height * 0.08
        anchors.horizontalCenter: parent.horizontalCenter
        text: control.gLabel
        color: control.blueColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.round(Math.min(parent.height * 0.18, parent.width * 0.13))
        opacity: 0.95
    }

    MouseArea {
        id: hitArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: control.activated()
    }
}
