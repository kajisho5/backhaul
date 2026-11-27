import QtQuick
import QtQuick.Layouts
import Backhaul

// What the venue is sending back to Zoom.
Rectangle {
    implicitHeight: column.implicitHeight + 4
    color: Theme.panel
    border.width: 2
    border.color: Theme.line
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
            Text { anchors.centerIn: parent; text: "本番映像"; color: "#6f757d"; font.family: app.sansFamily; font.pixelSize: 12; font.letterSpacing: 1.5 }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 10
            Text { text: "返"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            Text { Layout.fillWidth: true; text: "Zoomへの返し"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
            Text {
                readonly property bool allOk: app.returnVideoOk && app.returnAudioOk
                text: allOk ? "映像・音声 入力あり"
                            : (!app.returnVideoOk && !app.returnAudioOk ? "入力なし"
                               : (app.returnVideoOk ? "音声の入力なし" : "映像の入力なし"))
                color: allOk ? Theme.ok : Theme.lost
                font.family: app.sansFamily; font.pixelSize: 12; font.bold: true
            }
        }
    }
}
