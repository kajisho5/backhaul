import QtQuick
import QtQuick.Layouts
import Backhaul

// Right-hand panel of the all-in-one role while the return tile is selected.
Item {
    id: root
    property string state: "off"   // see Main.qml
    signal settingsRequested()
    implicitHeight: col.implicitHeight
    readonly property bool on: state !== "off"
    readonly property bool picture: returning.deviceIndex > 0 || app.demo
    readonly property bool sound: audio.micIndex > 0 || app.demo
    readonly property string waiting: app.connected ? qsTr("まだ送れていません") : qsTr("会議に入っていません")
    readonly property string trouble: returning.problem.length > 0 ? returning.problem
                                      : audio.micProblem.length > 0 ? audio.micProblem : ""
    readonly property string remark: meeting.sendVideoNote.length > 0 ? meeting.sendVideoNote : meeting.sendNote

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 4

        Text { text: qsTr("選択中"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            text: qsTr("Zoomへの返し")
            color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 20; font.bold: true
        }
        Repeater {
            model: !root.on ? [
                { k: qsTr("状態"), v: qsTr("送らない設定"), mono: false }
            ] : [
                { k: qsTr("映像"), v: !root.picture ? qsTr("送りません") : returning.paused ? qsTr("止めています")
                                : root.state === "sending" ? qsTr("送信中") + (returning.sentPerSecond > 0 ? qsTr("・毎秒") + returning.sentPerSecond + qsTr("コマ") : "")
                                : root.waiting, mono: false },
                { k: qsTr("送り方"), v: app.returnAsShare ? qsTr("画面共有として") : qsTr("カメラとして"), mono: false },
                { k: qsTr("映像の入力"), v: app.demo ? qsTr("サンプルUSBキャプチャー") : (returning.deviceNames[returning.deviceIndex] || qsTr("使わない")), mono: false },
                { k: qsTr("入力の形"), v: returning.inputText.length > 0 ? returning.inputText : "—", mono: true },
                { k: qsTr("音声"), v: !root.sound ? qsTr("送りません") : returning.paused ? qsTr("止めています")
                                : root.state === "sending" ? qsTr("送信中") : root.waiting, mono: false },
                { k: qsTr("音声の入力"), v: app.demo ? qsTr("サンプル音声機器") : (audio.micNames[audio.micIndex] || qsTr("使わない")), mono: false }
            ]
            delegate: Item {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                Text { anchors.verticalCenter: parent.verticalCenter; text: modelData.k; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    width: parent.width - 96
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                    text: modelData.v
                    color: Theme.ink
                    font.family: modelData.mono ? app.monoFamily : app.sansFamily
                    font.pixelSize: 13
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
            }
        }
        ColumnLayout {
            visible: root.on
            Layout.fillWidth: true
            Layout.topMargin: 10
            spacing: 6
            Text { text: qsTr("送る音"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            LevelBar { Layout.fillWidth: true; label: "L"; level: audio.micLevels[0] }
            LevelBar { Layout.fillWidth: true; label: "R"; level: audio.micLevels[1] }
        }
        Text {
            visible: root.on && text.length > 0
            Layout.fillWidth: true
            Layout.topMargin: 8
            wrapMode: Text.WordWrap
            text: root.trouble.length > 0 ? root.trouble : root.remark
            color: root.trouble.length > 0 ? Theme.lost : Theme.muted
            font.family: app.sansFamily; font.pixelSize: 12
        }
        PlainButton {
            visible: root.on
            Layout.fillWidth: true
            Layout.topMargin: 14
            implicitHeight: 40
            textSize: 13
            text: returning.paused ? qsTr("送信を再開する") : qsTr("送信を止める")
            tint: returning.paused ? Theme.ok : Theme.lost
            onClicked: returning.paused = !returning.paused
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            wrapMode: Text.WordWrap
            text: !root.on ? qsTr("このPCから会場の映像と音をZoomへ送るには、設定の「返し」で「送る」にします。")
                           : qsTr("会議に入ったまま、返しの映像と音だけを止めます。Zoomの声を卓へ出すのは止まりません。")
            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
        }
        PlainButton {
            Layout.fillWidth: true
            Layout.topMargin: 10
            implicitHeight: 36
            textSize: 13
            text: qsTr("返しの設定を開く")
            onClicked: root.settingsRequested()
        }
    }
}
