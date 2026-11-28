import QtQuick
import QtQuick.Layouts
import Backhaul

Rectangle {
    id: bar
    signal settingsRequested()
    signal roleRequested()
    readonly property var roleNames: ({ all: qsTr("1台でぜんぶ"), camera: qsTr("カメラ（1人）"), twoshot: qsTr("2ショット"), slide: qsTr("スライド"), audio: qsTr("音声"), return: qsTr("返し（映像・音声）") })
    // The demo picture shows a good line.
    readonly property string lineState: app.demo ? "good" : meeting.line
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
            enabled: !meeting.busy && !app.locked
            Accessible.name: qsTr("役割を変える。今は") + (bar.roleNames[app.role] || "") + (enabled ? "" : app.locked ? qsTr("。ロック中は変えられません") : qsTr("。会議中は変えられません"))
            onClicked: bar.roleRequested()
        }
        // What is going on. This part gives way when the window is narrow; the controls to
        // its right always stay on the screen.
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0  // without this the texts inside would push the controls off the screen
            Layout.preferredWidth: 0
            spacing: 18
            clip: true
        Text { text: app.meetingName; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; visible: text.length > 0 }
        Row {
            spacing: 6
            Text {
                text: app.connected ? qsTr("接続中") : meeting.rejoining ? qsTr("切断") : qsTr("未接続")
                color: app.connected ? Theme.ok : meeting.rejoining ? Theme.unstable : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
            Text { text: app.elapsed; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13; visible: app.connected }
        }
        // This PC's own line, while in the meeting.
        Row {
            spacing: 6
            visible: bar.lineState.length > 0
            Text { text: qsTr("回線"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            Text {
                text: bar.lineState === "good" ? qsTr("良好") : bar.lineState === "fair" ? qsTr("注意") : qsTr("不良")
                color: bar.lineState === "good" ? Theme.ok : bar.lineState === "fair" ? Theme.muted : Theme.lost
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
        }
        Row {
            spacing: 6
            visible: app.audioDevice.length > 0 && app.role === "all"
            Text { text: qsTr("音声"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            Text { text: app.audioDevice; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13 }
            Text {
                text: audio.onSpare ? qsTr("予備") : app.audioOk ? qsTr("正常") : qsTr("停止")
                color: audio.onSpare ? Theme.muted : app.audioOk ? Theme.ok : Theme.lost
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
        }

        Segmented {
            visible: app.role === "all"
            enabled: !app.locked
            opacity: enabled ? 1.0 : 0.4
            firstText: qsTr("1人1出力")
            secondText: qsTr("2×2まとめ")
            first: !app.gridMode
            onChosen: (first) => app.gridMode = !first
        }
        // The show lock. One press locks; unlocking takes a press held for a second, so that
        // a slip of the hand does not undo it.
        PlainButton {
            id: lockButton
            // A short press while locked says how to unlock, for two seconds.
            text: !app.locked ? qsTr("ロック") : hint.running ? qsTr("長押しで解除") : qsTr("ロック中")
            strong: app.locked
            property int held: 0  // key repeats counted while Return or Space stays down
            Timer { id: hint; interval: 2000 }
            Accessible.name: app.locked ? qsTr("ロック中。1秒長押しで解除") : qsTr("本番ロックをかける")
            Accessible.onPressAction: app.locked = !app.locked  // asked for on purpose by a screen reader
            onClicked: if (!app.locked) app.locked = true
            Keys.onPressed: (event) => {
                if (!app.locked || (event.key !== Qt.Key_Return && event.key !== Qt.Key_Space)) return
                if (!event.isAutoRepeat) hint.restart()
                held = event.isAutoRepeat ? held + 1 : 0
                if (held >= 20) { held = 0; app.locked = false }
                event.accepted = true
            }
            MouseArea {
                anchors.fill: parent
                enabled: app.locked
                pressAndHoldInterval: 1000
                onClicked: hint.restart()
                onPressAndHold: { hint.stop(); app.locked = false }
            }
        }
        PlainButton {
            text: qsTr("設定")
            enabled: !app.locked
            Accessible.name: updateDot.visible ? qsTr("設定（新しいバージョンがあります）") : text
            onClicked: bar.settingsRequested()
            // A newer version is waiting on the "Software" page of the settings.
            Rectangle {
                id: updateDot
                visible: updates.status === "available" && !meeting.busy
                anchors.right: parent.right; anchors.top: parent.top
                anchors.rightMargin: -3; anchors.topMargin: -3
                width: 8; height: 8
                color: Theme.ok
            }
        }
    }
}
