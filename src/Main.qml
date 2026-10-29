import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 500
    height: 620
    minimumWidth: 440
    minimumHeight: 600
    visible: true
    title: "ThinkBeat"
    color: "#101114"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property bool helpVisible: false
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.toggle() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: win.helpVisible; onActivated: win.helpVisible = false }
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 16
            Label { text: "ThinkBeat"; font.pixelSize: 30; font.bold: true }
            Label {
                text: qsTr("Give your ThinkPad a heartbeat.")
                color: "#a5a8b0"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                color: "#1a1c21"
                radius: theme.radius
                Column {
                    anchors.centerIn: parent
                    spacing: 18
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 38; height: 38; radius: 19
                        color: backend.lit ? "#ff354b" : "#49212a"
                        border.width: 1
                        border.color: backend.lit ? "#ff7b89" : "#66323d"
                        Behavior on color { ColorAnimation { duration: 35 } }
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: backend.preview ? qsTr("Preview · LED untouched") : qsTr("ThinkPad lid LED")
                        color: "#c4c6cc"
                    }
                }
            }
            Label { text: qsTr("Playback level"); color: "#a5a8b0" }
            ProgressBar { Layout.fillWidth: true; from: 0; to: 1; value: backend.level }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Sensitivity"); Layout.fillWidth: true }
                Label { text: { translator.language; return backend.sensitivity.toLocaleString(Qt.locale(), 'f', 1) + "×" } }
            }
            Slider {
                objectName: "sensitivitySlider"
                Layout.fillWidth: true
                from: 0.25; to: 8; value: backend.sensitivity
                onMoved: backend.sensitivity = value
            }
            Label {
                Layout.fillWidth: true
                text: backend.error.length ? backend.error : qsTr("Uses system playback, never your microphone. Stop and start after changing audio outputs.")
                color: backend.error.length ? "#ff8291" : "#a5a8b0"
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
            Item { Layout.fillHeight: true }
            Button {
                id: toggle
                objectName: "toggleButton"
                text: backend.running ? qsTr("Stop") : qsTr("Start listening")
                Layout.fillWidth: true
                highlighted: true
                background: Rectangle {
                    implicitHeight: 48
                    color: toggle.down ? Qt.darker(theme.accent, 1.15) : theme.accent
                    radius: theme.radius
                    border.width: toggle.activeFocus ? 2 : 0
                    border.color: "white"
                }
                contentItem: Label {
                    text: toggle.text
                    color: theme.accentForeground
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }
                onClicked: backend.toggle()
            }
            Label {
                text: qsTr("Space · start / stop     ? · shortcuts     Q · quit")
                font.pixelSize: 12
                color: "#868b98"
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
    Popup {
        anchors.centerIn: parent
        width: Math.min(win.width - 32, 390)
        visible: win.helpVisible
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: win.helpVisible = false
        contentItem: Label {
            text: qsTr("Keyboard shortcuts\n\nSpace — Start / stop\n? — Show / hide shortcuts\nQ — Quit\nEsc — Close this help")
            padding: 20
            wrapMode: Text.WordWrap
        }
    }
}
