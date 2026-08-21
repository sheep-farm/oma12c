import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: 800
    height: 720
    minimumWidth: 480
    minimumHeight: 540
    visible: true
    title: "OMA12C"

    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: backend.themeBackground
    readonly property color inkColor: backend.themeForeground
    readonly property color accentColor: backend.themeAccent

    readonly property real uiScale: Math.min(width / 800, height / 720)
    property string activeKey: ""

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * uiScale));
    }

    function mapToButtonKey(text) {
        if (text === "+") return "+";
        if (text === "-") return "−";
        if (text === "*") return "×";
        if (text === "/") return "÷";
        if (text === "=" || text === "\r" || text === "\n") return "ENTER";
        if (text === ".") return ".";
        if (text === "e" || text === "E") return "EEX";
        if (text === "c" || text === "C") return "CLx";
        if (text.toLowerCase() === "f") return "f";
        if (text.toLowerCase() === "g") return "g";
        if (text.toLowerCase() === "n") return "n";
        if (text.toLowerCase() === "i") return "i";
        return text;
    }

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: accentColor
    color: pageColor

    Shortcut {
        sequences: ["Ctrl+C", "Meta+C"]
        context: Qt.ApplicationShortcut
        onActivated: backend.copyResult()
    }

    Shortcut {
        sequences: ["Ctrl+V", "Meta+V"]
        context: Qt.ApplicationShortcut
        onActivated: backend.pasteNumber()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Item {
        id: face
        anchors.fill: parent
        anchors.margins: win.scaledSize(20)
        focus: true

        Keys.onPressed: function(event) {
            if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                return;

            const mapped = mapToButtonKey(event.text);
            if (mapped.length > 0) {
                win.activeKey = mapped;
                backend.pressKey(mapped);
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                win.activeKey = "ENTER";
                backend.pressKey("ENTER");
            } else if (event.key === Qt.Key_Backspace) {
                win.activeKey = "CLx";
                backend.pressKey("CLx");
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Delete) {
                win.activeKey = "CLx";
                backend.pressKey("CLx");
            } else {
                return;
            }
            event.accepted = true;
        }

        Keys.onReleased: function(event) {
            win.activeKey = "";
        }

        // Display with Omacalc-style clean look.
        Item {
            id: displayArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: win.scaledSize(120)

            // f annunciator, top left.
            Text {
                anchors.top: parent.top
                anchors.left: parent.left
                text: "f"
                color: mixColors(pageColor, Qt.rgba(0.93, 0.72, 0.18, 1), 0.85)
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(14)
                font.bold: true
                visible: backend.prefix === "f"
            }

            // g annunciator, bottom left.
            Text {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                text: "g"
                color: mixColors(pageColor, Qt.rgba(0.32, 0.58, 0.86, 1), 0.85)
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(14)
                font.bold: true
                visible: backend.prefix === "g"
            }

            Row {
                id: annunciators
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.leftMargin: win.scaledSize(24)
                spacing: win.scaledSize(10)

                Text {
                    text: "BEGIN"
                    color: inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(12)
                    opacity: 0.7
                    visible: backend.beginMode
                }

                Text {
                    text: "D.MY"
                    color: inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(12)
                    opacity: 0.7
                    visible: backend.dmyMode
                }

                Text {
                    text: "PRGM"
                    color: inkColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(12)
                    opacity: 0.7
                    visible: backend.programMode
                }
            }

            Text {
                id: displayText
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.left: parent.left
                text: backend.display
                color: inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(72)
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: win.scaledSize(28)
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignBottom
            }
        }

        // HP-12C classic 4x10 keypad with a double-height ENTER key.
        GridLayout {
            id: keypad
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: displayArea.bottom
            anchors.topMargin: win.scaledSize(10)
            columns: 10
            rowSpacing: win.scaledSize(8)
            columnSpacing: win.scaledSize(10)

            Repeater {
                model: [
                    // Section labels row.
                    { r: 0, c: 0, cs: 2, label: "BOND", kind: "section" },
                    { r: 0, c: 2, cs: 3, label: "DEPRECIATION", kind: "section" },

                    // Row 1
                    { r: 1, c: 0, label: "n", f: "AMORT", g: "12x", key: "n" },
                    { r: 1, c: 1, label: "i", f: "INT", g: "12÷", key: "i" },
                    { r: 1, c: 2, label: "PV", f: "NPV", g: "CFo", key: "PV" },
                    { r: 1, c: 3, label: "PMT", f: "RND", g: "CFj", key: "PMT" },
                    { r: 1, c: 4, label: "FV", f: "IRR", g: "Nj", key: "FV" },
                    { r: 1, c: 5, label: "CHS", f: "", g: "DATE", key: "CHS" },
                    { r: 1, c: 6, label: "7", f: "", g: "BEG", key: "7" },
                    { r: 1, c: 7, label: "8", f: "", g: "END", key: "8" },
                    { r: 1, c: 8, label: "9", f: "", g: "MEM", key: "9" },
                    { r: 1, c: 9, label: "÷", f: "", g: "", key: "÷" },

                    // Section labels row.
                    { r: 2, c: 1, cs: 4, label: "CLEAR", kind: "section" },

                    // Row 2
                    { r: 3, c: 0, label: "y^x", f: "PRICE", g: "√x", key: "y^x" },
                    { r: 3, c: 1, label: "1/x", f: "YTM", g: "e^x", key: "1/x" },
                    { r: 3, c: 2, label: "%T", f: "SL", g: "LN", key: "%T" },
                    { r: 3, c: 3, label: "Δ%", f: "SOYD", g: "FRAC", key: "Δ%" },
                    { r: 3, c: 4, label: "%", f: "DB", g: "INTG", key: "%" },
                    { r: 3, c: 5, label: "EEX", f: "FRAC", g: "ΔDYS", key: "EEX" },
                    { r: 3, c: 6, label: "4", f: "", g: "D.MY", key: "4" },
                    { r: 3, c: 7, label: "5", f: "", g: "M.DY", key: "5" },
                    { r: 3, c: 8, label: "6", f: "", g: "x↔w", key: "6" },
                    { r: 3, c: 9, label: "×", f: "", g: "", key: "×" },

                    // Row 3
                    { r: 4, c: 0, label: "R/S", f: "P/R", g: "PSE", key: "R/S" },
                    { r: 4, c: 1, label: "SST", f: "Σ", g: "BST", key: "SST" },
                    { r: 4, c: 2, label: "R↓", f: "PRGM", g: "GTO", key: "R↓" },
                    { r: 4, c: 3, label: "x<>y", f: "FIN", g: "x≤y", key: "x<>y" },
                    { r: 4, c: 4, label: "CLx", f: "REG", g: "x=0", key: "CLx" },
                    { r: 4, c: 5, rs: 2, label: "ENTER", f: "PREFIX", g: "LSTx", key: "ENTER", kind: "blue" },
                    { r: 4, c: 6, label: "1", f: "", g: "x̂,r", key: "1" },
                    { r: 4, c: 7, label: "2", f: "", g: "ŷ,r", key: "2" },
                    { r: 4, c: 8, label: "3", f: "", g: "n!", key: "3" },
                    { r: 4, c: 9, label: "−", f: "", g: "", key: "−" },

                    // Row 4
                    { r: 5, c: 0, label: "ON", f: "", g: "", key: "ON", kind: "black" },
                    { r: 5, c: 1, label: "f", f: "", g: "", key: "f", kind: "gold" },
                    { r: 5, c: 2, label: "g", f: "", g: "", key: "g", kind: "blue" },
                    { r: 5, c: 3, label: "STO", f: "", g: "", key: "STO" },
                    { r: 5, c: 4, label: "RCL", f: "", g: "", key: "RCL" },
                    // Column 5 occupied by the double-height ENTER above.
                    { r: 5, c: 6, label: "0", f: "", g: "x̄", key: "0" },
                    { r: 5, c: 7, label: ".", f: "", g: "s", key: "." },
                    { r: 5, c: 8, label: "Σ+", f: "", g: "Σ−", key: "Σ+" },
                    { r: 5, c: 9, label: "+", f: "", g: "", key: "+" }
                ]

                Loader {
                    sourceComponent: modelData.kind === "section" ? sectionLabelComponent : calcButtonComponent
                    Component {
                        id: sectionLabelComponent
                        SectionLabel {
                            Layout.row: modelData.r
                            Layout.column: modelData.c
                            Layout.columnSpan: modelData.cs || 1
                            Layout.rowSpan: 1
                            text: modelData.label
                            inkColor: win.inkColor
                            pageColor: win.pageColor
                        }
                    }
                    Component {
                        id: calcButtonComponent
                        CalcButton {
                            Layout.row: modelData.r
                            Layout.column: modelData.c
                            Layout.rowSpan: modelData.rs || 1
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            label: modelData.label
                            fLabel: modelData.f
                            gLabel: modelData.g
                            keyValue: modelData.key
                            kind: modelData.kind || "white"
                            pageColor: win.pageColor
                    inkColor: win.inkColor
                    accentColor: win.accentColor
                    activeKey: win.activeKey
                    prefix: backend.prefix
                    onActivated: backend.pressKey(modelData.key)
                }
            }
        }
    }

    function mixColors(base, tint, amount) {
        return Qt.rgba(
            base.r + (tint.r - base.r) * amount,
            base.g + (tint.g - base.g) * amount,
            base.b + (tint.b - base.b) * amount, 1);
    }

    // Remember the last windowed geometry.
    property rect normalGeometry: Qt.rect(x, y, width, height)
    property bool wasMaximized: false

    function trackNormalGeometry() {
        if (visibility === Window.Windowed)
            normalGeometry = Qt.rect(x, y, width, height);
    }

    onXChanged: trackNormalGeometry()
    onYChanged: trackNormalGeometry()
    onWidthChanged: trackNormalGeometry()
    onHeightChanged: trackNormalGeometry()

    onVisibilityChanged: {
        if (visibility === Window.Maximized || visibility === Window.FullScreen)
            wasMaximized = true;
        else if (visibility === Window.Windowed)
            wasMaximized = false;
    }

    Component.onCompleted: {
        var geometry = backend.windowGeometry();
        if (geometry.valid) {
            x = geometry.x;
            y = geometry.y;
            width = geometry.width;
            height = geometry.height;
            if (geometry.maximized) showMaximized();
        } else {
            width = Math.round(800 * backend.textScale);
            height = Math.round(720 * backend.textScale);
        }
    }

    Component.onDestruction: backend.saveWindowGeometry(
        normalGeometry.x, normalGeometry.y,
        normalGeometry.width, normalGeometry.height, wasMaximized)
}
