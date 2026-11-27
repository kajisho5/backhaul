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
            required property bool isShare
            required property string state
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
            // Before there is anything to show (nobody has shared yet, or the person is not in
            // the meeting yet): a thin frame that shows the picture's edges, the output's name,
            // and that it is waiting. Lets the switcher see which input this is.
            Item {
                id: standby
                // The slide output has a word of its own; a person's output shows that person's name.
                readonly property string word: one.isShare ? app.standbyText : one.label
                readonly property int wordSize: Math.max(10, Math.round(one.height * 0.065))
                readonly property int noteSize: Math.max(8, Math.round(one.height * 0.032))
                readonly property int edge: Math.round(one.height * 0.075)
                anchors.fill: parent
                visible: one.state === "unassigned" && word.length > 0
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Math.round(one.height * 0.035)
                    color: "transparent"
                    border.width: Math.max(1, Math.round(one.height / 540))
                    border.color: "#3a3e44"
                }
                Text {
                    x: standby.edge
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Math.round(one.height * 0.065)
                    width: Math.max(0, one.width - standby.edge * 2 - waiting.width - one.height * 0.04)
                    elide: Text.ElideRight
                    text: standby.word
                    color: Theme.ink
                    font.family: one.isShare ? app.monoFamily : app.sansFamily
                    font.pixelSize: standby.wordSize
                    font.bold: true
                    font.letterSpacing: one.isShare ? standby.wordSize * 0.16 : 0
                }
                Text {
                    id: waiting
                    anchors.right: parent.right
                    anchors.rightMargin: standby.edge
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: standby.edge
                    text: one.isShare ? "WAITING FOR SHARE" : "WAITING"
                    color: Theme.mute
                    font.family: app.monoFamily
                    font.pixelSize: standby.noteSize
                    font.letterSpacing: standby.noteSize * 0.08
                }
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
