import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Window {
    width: 900
    height: 600
    visible: true
    title: "Wunda Vanguard - Network Controller"
    color: "#050505"

    // Custom Font & Colors
    readonly property color primaryCyan: "#00f2ff"
    readonly property color accentPurple: "#8a2be2"
    readonly property color bgDark: "#0a0a0a"

    // Background Gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0d0d1a" }
            GradientStop { position: 1.0; color: "#050505" }
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 25

        // Header
        Row {
            width: parent.width
            height: 60
            spacing: 15

            Rectangle {
                width: 50; height: 50; radius: 10
                color: "transparent"
                border.color: primaryCyan
                border.width: 2
                Text {
                    anchors.centerIn: parent
                    text: "WV"
                    color: primaryCyan
                    font.pixelSize: 22
                    font.bold: true
                }
            }

            Column {
                Text {
                    text: "WUNDA VANGUARD"
                    color: "white"
                    font.pixelSize: 24
                    font.bold: true
                    letterSpacing: 2
                }
                Text {
                    text: "Network Security & Traffic Control"
                    color: "#666"
                    font.pixelSize: 12
                }
            }

            Item { width: 1; height: 1; Layout.fillWidth: true }

            Button {
                id: scanBtn
                text: "SCAN NETWORK"
                contentItem: Text {
                    text: scanBtn.text
                    color: scanBtn.down ? primaryCyan : "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }
                background: Rectangle {
                    implicitWidth: 150
                    implicitHeight: 40
                    color: scanBtn.down ? "#11ffffff" : "transparent"
                    border.color: primaryCyan
                    border.width: 1
                    radius: 5
                }
                onClicked: backend.startScan()
            }
        }

        // Device List
        ListView {
            width: parent.width
            height: parent.height - 150
            spacing: 15
            clip: true
            model: backend.devices

            delegate: Rectangle {
                width: parent.width
                height: 100
                color: "#151515"
                radius: 12
                border.color: modelData.isSpoofing ? primaryCyan : "#222"
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 20

                    Column {
                        width: 200
                        Text { text: modelData.hostname; color: "white"; font.pixelSize: 18; font.bold: true }
                        Text { text: modelData.ip; color: "#aaa"; font.pixelSize: 13 }
                        Text { text: modelData.mac; color: "#666"; font.pixelSize: 11 }
                    }

                    Column {
                        width: 150
                        Text { text: "DOWN: " + modelData.downSpeed; color: primaryCyan; font.pixelSize: 14; font.bold: true }
                        Text { text: "UP: " + modelData.upSpeed; color: accentPurple; font.pixelSize: 14; font.bold: true }
                    }

                    Column {
                        id: controlCol
                        width: 300
                        spacing: 5
                        Text { text: "LIMIT: " + (limitSlider.value == 5000 ? "UNLIMITED" : limitSlider.value + " KB/s"); color: "white"; font.pixelSize: 11 }
                        Slider {
                            id: limitSlider
                            width: 280
                            from: 50
                            to: 5000
                            value: modelData.throttle
                            onMoved: backend.setThrottle(modelData.ip, value)
                        }
                    }

                    Switch {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: modelData.isSpoofing
                        onToggled: backend.toggleSpoof(modelData.ip, checked)
                    }
                }
            }
        }
    }
}
