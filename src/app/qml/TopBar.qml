import QtQuick
import QtQuick.Layouts
import Backhaul

Rectangle {
    id: bar
    signal settingsRequested()
    signal roleRequested()
    readonly property var roleNames: ({ all: qsTr("1台でぜんぶ"), camera: qsTr("カメラ（1人）"), twoshot: qsTr("2ショット"), slide: qsTr("スライド"), audio: qsTr("音声"), return: qsTr("返し（映像・音声）") })
    color: Theme.bg
    implicitHeight: 48

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 18

        Text { text: "backhaul"; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1 }
        PlainButton {
            visible: app.role.length > 0
            text: qsTr("役割　") + (bar.roleNames[app.role] || "") + "　▼"
            textSize: 13
            implicitHeight: 32
            // The role decides how this PC joins (with or without sound), so it is fixed during a meeting.
            enabled: !meeting.busy
            Accessible.name: qsTr("役割を変える。今は") + (bar.roleNames[app.role] || "") + (enabled ? "" : qsTr("。会議中は変えられません"))
            onClicked: bar.roleRequested()
        }
        Text { text: app.meetingName; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; visible: text.length > 0 }
        Row {
            spacing: 6
            Text {
                text: app.connected ? qsTr("接続中") : qsTr("未接続")
                color: app.connected ? Theme.ok : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
            Text { text: app.elapsed; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13; visible: app.connected }
        }
        Row {
            spacing: 6
            visible: app.audioDevice.length > 0 && app.role === "all"
            Text { text: qsTr("音声"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            Text { text: app.audioDevice; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13 }
            Text {
                text: app.audioOk ? qsTr("正常") : qsTr("停止")
                color: app.audioOk ? Theme.ok : Theme.lost
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
        }
        Rectangle {
            visible: app.demo
            color: "transparent"; border.width: 1; border.color: Theme.muted; radius: 2
            implicitWidth: demoText.implicitWidth + 16; implicitHeight: 24
            Text { id: demoText; anchors.centerIn: parent; text: qsTr("デモ表示（Zoom未接続）"); color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
        }

        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: app.notice
            color: Theme.lost
            font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
        }

        Segmented {
            visible: app.role === "all"
            firstText: qsTr("1人1出力")
            secondText: qsTr("2×2まとめ")
            first: !app.gridMode
            onChosen: (first) => app.gridMode = !first
        }
        PlainButton { text: qsTr("設定"); onClicked: bar.settingsRequested() }
    }
}
