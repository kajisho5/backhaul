import QtQuick
import Backhaul
import Backhaul.Video

// What one monitor shows full screen: the live picture, the last picture, the name card, or black.
Rectangle {
    id: root
    property int slotRow: -1
    // Fill the area and cut what does not fit (two-shot halves); slides keep their own setting.
    property bool fill: false
    // Output windows hide the mouse pointer; previews on the operator screen do not.
    property bool hideCursor: true
    // Shown where the picture will be while none has arrived (previews only).
    property string placeholder: ""
    color: "black"

    Repeater {
        model: app.slotList
        delegate: Item {
            id: one
            required property int index
            required property string label
            required property string videoOut
            required property bool crop
            anchors.fill: parent
            visible: index === root.slotRow

            Text {
                visible: root.placeholder.length > 0 && (one.videoOut === "live" || one.videoOut === "freeze")
                anchors.centerIn: parent
                text: one.videoOut === "freeze" ? "静止" : root.placeholder
                color: "#6f757d"
                font.family: app.sansFamily; font.pixelSize: 13; font.letterSpacing: 1.5
            }
            FrameItem {
                anchors.fill: parent
                visible: one.videoOut === "live" || one.videoOut === "freeze"
                slot: one.index
                crop: one.crop || root.fill
            }
            Rectangle {
                anchors.fill: parent
                visible: one.videoOut === "card"
                color: "#1c1f23"
                Column {
                    anchors.centerIn: parent
                    spacing: root.height * 0.045
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: root.width * 0.22
                        height: width
                        radius: width / 2
                        color: "#34383e"
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(implicitWidth, root.width * 0.9)
                        elide: Text.ElideRight
                        text: one.label
                        color: Theme.ink
                        font.family: app.sansFamily
                        font.pixelSize: Math.max(12, root.height * 0.1)
                        font.bold: true
                    }
                }
            }
        }
    }
    MouseArea { anchors.fill: parent; visible: root.hideCursor; cursorShape: Qt.BlankCursor; acceptedButtons: Qt.NoButton }
}
