import QtQuick
import QtQuick.Layouts
import Backhaul

// Overview of the audio assignment: slot -> channel, level and delay.
Rectangle {
    implicitHeight: column.implicitHeight + 28
    color: Theme.panel
    border.width: 1
    border.color: Theme.line
    radius: 2

    ColumnLayout {
        id: column
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14; leftMargin: 16; rightMargin: 16 }
        spacing: 12
        Text { text: "音声の割り当て"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 20
            Repeater {
                model: app.slotList
                delegate: ColumnLayout {
                    required property string number
                    required property string label
                    required property int channel
                    required property double level
                    required property double gainDb
                    required property int delayMs
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: number + "　" + label; elide: Text.ElideRight; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 12 }
                        Text { text: channel > 0 ? "ch " + channel : "—"; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12 }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        color: "#1d2024"
                        Rectangle { height: parent.height; width: parent.width * Math.max(0, Math.min(1, level)); color: Theme.ok }
                    }
                    Text {
                        text: (gainDb > 0 ? "+" : "") + gainDb.toFixed(1) + " dB・" + delayMs + " ms"
                        color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 11
                    }
                }
            }
        }
    }
}
