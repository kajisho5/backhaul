import QtQuick
import QtQuick.Layouts
import Backhaul

Rectangle {
    color: Theme.bg
    implicitHeight: 48

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 18

        Text { text: "backhaul"; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1 }
        Text { text: app.meetingName; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; visible: text.length > 0 }
        Row {
            spacing: 6
            Text {
                text: app.connected ? "接続中" : "未接続"
                color: app.connected ? Theme.ok : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
            Text { text: app.elapsed; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13; visible: app.connected }
        }
        Row {
            spacing: 6
            visible: app.audioDevice.length > 0
            Text { text: "音声"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            Text { text: app.audioDevice; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13 }
            Text {
                text: app.audioOk ? "正常" : "停止"
                color: app.audioOk ? Theme.ok : Theme.lost
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
        }
        Rectangle {
            visible: app.demo
            color: "transparent"; border.width: 1; border.color: Theme.muted; radius: 2
            implicitWidth: demoText.implicitWidth + 16; implicitHeight: 24
            Text { id: demoText; anchors.centerIn: parent; text: "デモ表示（Zoom未接続）"; color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
        }

        Item { Layout.fillWidth: true }

        Segmented {
            firstText: "1人1出力"
            secondText: "2×2まとめ"
            first: !app.gridMode
            onChosen: (first) => app.gridMode = !first
        }
    }
}
