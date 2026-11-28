import QtQuick
import QtQuick.Layouts
import Backhaul
import Backhaul.Video

// Return: what this PC sends into the meeting (the picture of a capture device, the
// sound of an input). The same choices as on the return role's own screen.
ColumnLayout {
    id: root
    spacing: 0
    readonly property bool pictureOn: app.sendsVideo
    readonly property bool soundOn: app.sendsAudio
    readonly property string roleLine: app.role === "return" ? qsTr("今の役割「返し」：いつも送ります")
        : app.role === "all" ? (app.returnInAll ? qsTr("今の役割「1台でぜんぶ」：送ります") : qsTr("今の役割「1台でぜんぶ」：送りません"))
        : app.role === "audio" ? qsTr("今の役割「音声」：音声だけ送ります（映像は送りません）")
        : qsTr("今の役割では、返しは送りません。機器は、送る役割にすると選べます")

    Text { text: qsTr("返し"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 6 }
    Text {
        Layout.fillWidth: true
        Layout.bottomMargin: 22
        wrapMode: Text.WordWrap
        text: qsTr("会場の映像と音をZoomへ送る設定です。役割「返し」と「1台でぜんぶ」で使います。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }

    // ---- the all-in-one switch ----
    Rectangle {
        Layout.fillWidth: true
        Layout.bottomMargin: 16
        implicitHeight: allRow.implicitHeight + 28
        color: Theme.panel
        border.width: 1
        border.color: Theme.line
        RowLayout {
            id: allRow
            x: 16; y: 14
            width: parent.width - 32
            spacing: 20
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: qsTr("「1台でぜんぶ」のPCから返しを送る"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("「送る」にすると、受けと返しを1台で行います。役割「返し」のPCは、ここに関係なくいつも送ります。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                }
            }
            Segmented {
                implicitHeight: 36
                firstText: qsTr("送る")
                secondText: qsTr("送らない")
                first: app.returnInAll
                onChosen: (first) => app.returnInAll = first
            }
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.bottomMargin: 22
        wrapMode: Text.WordWrap
        text: root.roleLine
        color: root.pictureOn ? Theme.ok : Theme.muted
        font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 40
        z: 20

        // ---- picture ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: 8
            z: videoPick.open ? 10 : 0
            opacity: root.pictureOn ? 1.0 : 0.45
            Text { text: qsTr("映像"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 16; font.bold: true; Layout.bottomMargin: 4 }
            Text { text: qsTr("送り方"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Segmented {
                implicitHeight: 36
                enabled: root.pictureOn
                firstText: qsTr("カメラとして")
                secondText: qsTr("画面共有として")
                first: !app.returnAsShare
                onChosen: (first) => app.returnAsShare = !first
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: app.returnAsShare ? qsTr("共有画面として大きく見えます。毎秒60コマまで送ります。ほかの人が共有している間は送れません（演者がスライドを共有する会では「カメラとして」にしてください）。")
                                        : qsTr("参加者の1人の映像として見えます。毎秒30コマまでです（Zoomのカメラ映像の上限）。")
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
            }
            Text { Layout.topMargin: 8; text: qsTr("映像の入力（USBキャプチャー）"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                z: videoPick.open ? 10 : 0
                Dropdown {
                    id: videoPick
                    Layout.fillWidth: true
                    name: qsTr("映像の入力")
                    enabled: root.pictureOn && returning.available
                    options: returning.deviceNames
                    current: returning.deviceIndex
                    dimmed: returning.deviceIndex === 0
                    onPicked: (i) => returning.chooseDevice(i)
                }
                PlainButton {
                    visible: returning.available
                    enabled: root.pictureOn
                    text: qsTr("探し直す")
                    implicitHeight: 40
                    Accessible.name: qsTr("映像の機器を探し直す")
                    onClicked: returning.refreshDevices()
                }
            }
            Text {
                visible: text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: returning.problem.length > 0 ? returning.problem
                      : (!returning.available && !app.demo) ? qsTr("このPCには映像を取り込める機器がありません。") : ""
                color: returning.problem.length > 0 ? Theme.lost : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 12
            }
            Rectangle {
                Layout.topMargin: 8
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                Layout.preferredHeight: Math.min(width, 420) * 9 / 16
                color: Theme.picture
                border.width: 1
                border.color: Theme.line
                Text {
                    anchors.centerIn: parent
                    text: returning.deviceIndex > 0 || app.demo ? qsTr("入力の映像（確認用）") : qsTr("映像の入力を選ぶと、ここに映ります")
                    color: "#6f757d"; font.family: app.sansFamily; font.pixelSize: 12; font.letterSpacing: 1.5
                }
                FrameItem { anchors.fill: parent; anchors.margins: 1; slot: 1000; visible: root.pictureOn }
                Rectangle {
                    visible: returning.inputText.length > 0
                    x: 9; y: 9
                    color: "#b3000000"
                    width: inLabel.implicitWidth + 12; height: inLabel.implicitHeight + 4
                    Text { id: inLabel; anchors.centerIn: parent; text: qsTr("入力 ") + returning.inputText; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 11 }
                }
            }
        }

        // ---- sound ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: 8
            z: micPick.open || qualityPick.open ? 10 : 0
            opacity: root.soundOn ? 1.0 : 0.45
            Text { text: qsTr("音声"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 16; font.bold: true; Layout.bottomMargin: 4 }
            Text { text: qsTr("音声の入力"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                z: micPick.open ? 10 : 0
                Dropdown {
                    id: micPick
                    Layout.fillWidth: true
                    name: qsTr("音声の入力")
                    enabled: root.soundOn && audio.micAvailable
                    options: audio.micNames
                    current: audio.micIndex
                    dimmed: audio.micIndex === 0
                    onPicked: (i) => audio.chooseMic(i)
                }
                PlainButton {
                    visible: audio.micAvailable && !app.audioAsio
                    enabled: root.soundOn
                    text: qsTr("探し直す")
                    implicitHeight: 40
                    Accessible.name: qsTr("音声の機器を探し直す")
                    onClicked: audio.refreshMics()
                }
            }
            LevelBar { Layout.fillWidth: true; Layout.topMargin: 4; label: "L"; level: root.soundOn ? audio.micLevels[0] : 0 }
            LevelBar { Layout.fillWidth: true; label: "R"; level: root.soundOn ? audio.micLevels[1] : 0 }
            PluginRack { Layout.fillWidth: true; Layout.topMargin: 6; target: -1; enabled: root.soundOn }
            Text { Layout.topMargin: 6; text: qsTr("音質"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Dropdown {
                id: qualityPick
                Layout.fillWidth: true
                z: open ? 10 : 0
                name: qsTr("Zoomへ送る音質")
                enabled: root.soundOn
                options: [qsTr("会話向け（Zoomの標準の加工）"), qsTr("原音（加工なし）"), qsTr("原音・高音質（音楽向け）")]
                current: app.sendQuality
                onPicked: (i) => app.sendQuality = i
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Text { Layout.fillWidth: true; text: qsTr("チャンネル"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                Segmented {
                    enabled: root.soundOn && app.sendQuality > 0
                    opacity: enabled ? 1.0 : 0.4
                    firstText: qsTr("モノラル")
                    secondText: qsTr("ステレオ")
                    first: !app.sendStereo
                    onChosen: (first) => app.sendStereo = !first
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                wrapMode: Text.WordWrap
                text: audio.micProblem.length > 0 ? audio.micProblem
                      : app.audioAsio && audio.micNames.length < 2 ? qsTr("ASIOの入力は、「音声出力」でASIOの機器を選ぶとここに並びます。")
                      : qsTr("送るのは、演者の声を抜いた会場の音（マイナスワン）にしてください。Zoomの声が混ざった音を送ると、演者に自分の声が遅れて返ります。")
                color: audio.micProblem.length > 0 ? Theme.lost : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 12
            }
        }
    }

    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; Layout.topMargin: 24; color: Theme.line }
    Text {
        Layout.fillWidth: true
        Layout.topMargin: 12
        wrapMode: Text.WordWrap
        text: meeting.busy ? qsTr("会議に入っています。機器・送り方・音質の変更と「送る」は、次に会議へ入るときから効きます（「送らない」はすぐに止まります）。")
                           : qsTr("機器・送り方・音質の変更は、次に会議へ入るときから効きます。")
        color: meeting.busy ? Theme.muted : Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
    }
}
