import QtQuick
import QtQuick.Layouts
import Backhaul

// One slot on the monitor wall: picture area on top, name bar below.
Item {
    id: root
    required property int index
    required property string number
    required property string label
    required property string state
    required property string stateText
    required property bool alsoMuted
    required property string videoOut
    required property bool isShare
    required property string sizeText
    required property double level

    property bool quiet: false
    signal picked()
    readonly property bool selected: !quiet && app.selected === index
    readonly property color tint: Theme.stateColor(state)

    implicitHeight: body.implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: "枠" + number + " " + label + " " + stateText
    Accessible.onPressAction: { app.selected = index; root.picked() }
    Keys.onReturnPressed: { app.selected = index; root.picked() }
    Keys.onSpacePressed: { app.selected = index; root.picked() }

    // Selection ring, drawn outside the tile so the state colour stays visible.
    Rectangle {
        visible: root.selected || root.activeFocus
        anchors.fill: parent
        anchors.margins: -6
        color: "transparent"
        border.width: root.selected ? 3 : 1
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
                color: root.videoOut === "card" ? "#1c1f23" : Theme.picture

                // Stand-in until real pictures arrive from Zoom.
                Text {
                    visible: root.videoOut === "live" || root.videoOut === "freeze"
                    anchors.centerIn: parent
                    text: root.videoOut === "freeze" ? "静止" : (root.isShare ? "スライド" : "映像")
                    color: "#6f757d"
                    font.family: app.sansFamily; font.pixelSize: 12; font.letterSpacing: 1.5
                }
                Column {
                    visible: root.videoOut === "card"
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle { width: 56; height: 56; radius: 28; color: "#34383e"; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: root.label; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 20; font.bold: true }
                }

                Rectangle {
                    x: 8; y: 8
                    color: "#b3000000"
                    width: sizeLabel.implicitWidth + 12; height: sizeLabel.implicitHeight + 4
                    Text { id: sizeLabel; anchors.centerIn: parent; text: root.sizeText; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 11 }
                }
                Rectangle {
                    Accessible.role: Accessible.Indicator
                    Accessible.name: "音量メーター"
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 8 }
                    width: 6
                    color: "#1d2024"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * Math.max(0, Math.min(1, root.level))
                        color: Theme.ok
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: root.selected ? Theme.ink : "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Text { text: root.number; color: root.selected ? "#3a3d42" : Theme.mute; font.family: app.monoFamily; font.pixelSize: 13 }
                    Text {
                        Layout.fillWidth: true
                        text: root.label
                        elide: Text.ElideRight
                        color: root.selected ? Theme.bg : Theme.ink
                        font.family: app.sansFamily; font.pixelSize: 15; font.bold: true
                    }
                    Rectangle {
                        visible: root.alsoMuted
                        color: Theme.muted
                        implicitWidth: alsoText.implicitWidth + 16; implicitHeight: 22
                        Text { id: alsoText; anchors.centerIn: parent; text: "ミュート"; color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
                    }
                    Rectangle {
                        color: root.tint
                        implicitWidth: chipText.implicitWidth + 16; implicitHeight: 22
                        Text { id: chipText; anchors.centerIn: parent; text: root.stateText; color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
                    }
                }
            }
        }
    }

    MouseArea { anchors.fill: parent; onClicked: { app.selected = root.index; root.picked(); root.forceActiveFocus() } }
}
