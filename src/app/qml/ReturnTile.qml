import QtQuick
import QtQuick.Layouts
import Backhaul
import Backhaul.Video

// What the venue is sending back to Zoom, on the wall of the all-in-one role.
Item {
    id: root
    property bool picked: false
    // "off" | "paused" | "sending" | "noinput" | "idle" (see Main.qml)
    property string state: "off"
    signal chosen()
    readonly property bool on: state !== "off"
    readonly property color tint: state === "sending" ? Theme.ok : state === "paused" || state === "idle" ? Theme.muted
                                  : state === "noinput" ? Theme.lost : Theme.line
    readonly property string stateText: state === "sending" ? qsTr("送信中") : state === "paused" ? qsTr("停止中")
                                        : state === "noinput" ? qsTr("入力なし") : state === "idle" ? qsTr("未送信") : qsTr("送らない")
    implicitHeight: body.implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Zoomへの返し ") + stateText
    Accessible.onPressAction: root.chosen()
    Keys.onReturnPressed: root.chosen()
    Keys.onSpacePressed: root.chosen()

    Rectangle {
        visible: root.picked || root.activeFocus
        anchors.fill: parent
        anchors.margins: -6
        color: "transparent"
        border.width: root.picked ? 3 : 1
        border.color: Theme.ink
        radius: 3
    }

    Rectangle {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: column.implicitHeight + 4
        color: Theme.panel
        border.width: 2
        border.color: root.tint
        radius: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 2
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: width * 9 / 16
                color: Theme.picture
                Text {
                    anchors.centerIn: parent
                    text: !root.on ? qsTr("返しは送らない設定です")
                          : returning.deviceIndex === 0 && !app.demo ? qsTr("映像の入力を選んでいません") : qsTr("会場の映像")
                    color: "#6f757d"; font.family: app.sansFamily; font.pixelSize: 12; font.letterSpacing: 1.5
                }
                FrameItem { anchors.fill: parent; slot: 1000; visible: root.on }
                Rectangle {
                    visible: root.on && returning.inputText.length > 0
                    x: 8; y: 8
                    color: "#b3000000"
                    width: sizeLabel.implicitWidth + 12; height: sizeLabel.implicitHeight + 4
                    Text { id: sizeLabel; anchors.centerIn: parent; text: returning.inputText; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 11 }
                }
                Rectangle {
                    visible: root.on
                    x: 8; y: parent.height - height - 8
                    color: "#b3000000"
                    width: wayLabel.implicitWidth + 12; height: wayLabel.implicitHeight + 4
                    Text { id: wayLabel; anchors.centerIn: parent; text: app.returnAsShare ? qsTr("画面共有として") : qsTr("カメラとして"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 11 }
                }
                Rectangle {
                    visible: root.on
                    Accessible.role: Accessible.Indicator
                    Accessible.name: qsTr("送る音のメーター")
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 8 }
                    width: 6
                    color: "#1d2024"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * Math.max(0, Math.min(1, Math.max(audio.micLevels[0], audio.micLevels[1])))
                        color: Theme.ok
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: root.picked ? Theme.ink : "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Text { text: qsTr("返"); color: root.picked ? "#3a3d42" : Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                    Text { Layout.fillWidth: true; text: qsTr("Zoomへの返し"); elide: Text.ElideRight; color: root.picked ? Theme.bg : Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                    Rectangle {
                        color: root.on ? root.tint : "transparent"
                        border.width: root.on ? 0 : 1
                        border.color: root.picked ? "#3a3d42" : Theme.mute
                        implicitWidth: chip.implicitWidth + 16; implicitHeight: 22
                        Text { id: chip; anchors.centerIn: parent; text: root.stateText; color: root.on ? Theme.bg : (root.picked ? "#3a3d42" : Theme.mute); font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
                    }
                }
            }
        }
    }
    MouseArea { anchors.fill: parent; onClicked: { root.chosen(); root.forceActiveFocus() } }
}
