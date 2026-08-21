import QtQuick

// Thin section header used above the HP-12C keypad groups.
Rectangle {
    id: control

    property string text
    property color inkColor: "#eeeeee"
    property color pageColor: "#101010"

    color: "transparent"

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height * 0.15
        text: control.text
        color: control.inkColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.round(Math.min(parent.height * 0.55, parent.width * 0.14))
        font.bold: true
        opacity: 0.45
        horizontalAlignment: Text.AlignHCenter
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: parent.width * 0.05
        anchors.rightMargin: parent.width * 0.05
        height: Math.max(1, Math.round(parent.height * 0.08))
        color: control.inkColor
        opacity: 0.12
        radius: height / 2
    }
}
