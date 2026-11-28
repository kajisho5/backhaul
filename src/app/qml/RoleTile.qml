import QtQuick
import QtQuick.Layouts
import QtQml
import Backhaul

// The big preview of a single-output role: the picture as it goes out, and below it
// who is in it and how they are doing. The size follows the space that is free.
Item {
    id: root
    readonly property bool twoShot: app.role === "twoshot"
    readonly property bool slide: app.role === "slide"
    // The largest 16:9 picture that fits together with the name bar.
    readonly property real pictureWidth: Math.max(160, Math.min(width - 4, (height - 4 - bar.height) * 16 / 9))
    property color tint: Theme.line

    Rectangle {
        id: frame
        width: root.pictureWidth + 4
        height: root.pictureWidth * 9 / 16 + bar.height + 4
        color: Theme.panel
        border.width: 2
        border.color: root.tint
        radius: 2

        Item {
            id: picture
            x: 2; y: 2
            width: parent.width - 4
            height: width * 9 / 16
            OutputView { anchors.fill: parent; visible: !root.twoShot; slotRow: app.roleRow; hideCursor: false; placeholder: root.slide ? qsTr("共有画面") : qsTr("映像") }
            TwoShotView { anchors.fill: parent; visible: root.twoShot; hideCursor: false; placeholder: qsTr("映像") }
        }

        RowLayout {
            id: bar
            x: 14
            y: picture.y + picture.height
            width: parent.width - 28
            height: 44
            spacing: 14
            layoutDirection: root.twoShot && app.twoShotLeft === 1 ? Qt.RightToLeft : Qt.LeftToRight

            Repeater {
                model: app.slotList
                delegate: RowLayout {
                    id: one
                    required property int index
                    required property string label
                    required property string zoomName
                    required property string state
                    required property string stateText
                    required property string sizeText
                    required property string detailText
                    required property bool isShare
                    readonly property bool used: root.twoShot ? index < 2 : index === app.roleRow
                    visible: used
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    layoutDirection: Qt.LeftToRight
                    spacing: 10
                    // One person: the frame takes that person's state colour.
                    Binding { target: root; property: "tint"; value: Theme.stateColor(one.state); when: one.used && !root.twoShot }

                    Text {
                        Layout.fillWidth: true
                        text: one.zoomName.length > 0 ? one.zoomName : (one.isShare ? qsTr("共有なし") : qsTr("未選択"))
                        elide: Text.ElideRight
                        color: one.zoomName.length > 0 ? Theme.ink : Theme.mute
                        font.family: app.sansFamily; font.pixelSize: 16; font.bold: true
                    }
                    Text {
                        text: one.sizeText + (one.detailText.length > 0 ? "・" + one.detailText : "")
                        color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12
                    }
                    Rectangle {
                        visible: one.state !== "unassigned"
                        color: Theme.stateColor(one.state)
                        implicitWidth: badge.implicitWidth + 20
                        implicitHeight: 22
                        Text { id: badge; anchors.centerIn: parent; text: one.isShare && one.state === "lost" ? qsTr("共有停止") : one.stateText; color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
                    }
                }
            }
        }
    }
}
