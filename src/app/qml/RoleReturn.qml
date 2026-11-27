import QtQuick
import QtQuick.Layouts
import Backhaul
import Backhaul.Video

// Operator screen of the return role: the venue's programme picture (from a capture
// device) and, if wanted, the venue's sound go into the meeting.
RowLayout {
    id: root
    spacing: 0
    readonly property bool picking: returning.deviceIndex > 0
    readonly property color tint: meeting.sendingVideo && !returning.paused ? Theme.ok
                                  : root.picking ? Theme.muted : Theme.line

    ColumnLayout {
        id: main
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 20
        spacing: 14

        // The picture as it comes from the capture device, as large as the space allows.
        Item {
            id: tile
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(140, Math.min(main.height - statusBox.height - 14, (main.width - 4) * 9 / 16 + 48))
            readonly property real pictureWidth: Math.max(160, Math.min(width - 4, (height - 48) * 16 / 9))

            Rectangle {
                width: tile.pictureWidth + 4
                height: tile.pictureWidth * 9 / 16 + 48
                color: Theme.panel
                border.width: 2
                border.color: root.tint
                radius: 2

                Rectangle {
                    id: picture
                    x: 2; y: 2
                    width: parent.width - 4
                    height: width * 9 / 16
                    color: "black"
                    Text {
                        anchors.centerIn: parent
                        text: root.picking || app.demo ? "スイッチャーの出力（取り込み中の映像）" : "映像の機器を選んでください"
                        color: "#6f757d"
                        font.family: app.sansFamily; font.pixelSize: 13; font.letterSpacing: 1.5
                    }
                    FrameItem { anchors.fill: parent; slot: 1000 }
                    Rectangle {
                        visible: returning.inputText.length > 0
                        x: 10; y: 10
                        color: "#b3000000"
                        width: inputLabel.implicitWidth + 12; height: inputLabel.implicitHeight + 4
                        Text { id: inputLabel; anchors.centerIn: parent; text: "入力 " + returning.inputText; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 12 }
                    }
                }
                RowLayout {
                    x: 14
                    y: picture.y + picture.height
                    width: parent.width - 28
                    height: 44
                    spacing: 12
                    Text {
                        text: returning.paused ? "送信を止めています"
                              : meeting.sendingVideo ? "Zoomへ送信中"
                              : app.connected ? "まだ送れていません" : "会議に入っていません"
                        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 16; font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: app.returnAsShare ? "画面共有として" : "カメラとして"
                        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                    }
                    Rectangle {
                        visible: meeting.sendingVideo || returning.paused
                        color: returning.paused ? Theme.muted : Theme.ok
                        implicitWidth: sendBadge.implicitWidth + 20
                        implicitHeight: 22
                        Text { id: sendBadge; anchors.centerIn: parent; text: returning.paused ? "停止中" : "送信中"; color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true }
                    }
                }
            }
        }

        Rectangle {
            id: statusBox
            Layout.fillWidth: true
            implicitHeight: strip.implicitHeight + 24
            color: Theme.panel
            border.width: 1
            border.color: Theme.line
            Flow {
                id: strip
                x: 16; y: 12
                width: parent.width - 32
                spacing: 24
                Row {
                    spacing: 8
                    Text { text: "映像"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                    Text {
                        text: meeting.sendingVideo && !returning.paused ? "送信中" : "送っていません"
                        color: meeting.sendingVideo && !returning.paused ? Theme.ok : Theme.muted
                        font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                    }
                    Text {
                        visible: meeting.sendingVideo && !returning.paused
                        text: "毎秒" + returning.sentPerSecond + "コマ"
                        color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
                    }
                }
                Row {
                    spacing: 8
                    Text { text: "音声"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                    Text {
                        text: audio.micIndex === 0 ? "送りません" : (meeting.sendingAudio && !returning.paused ? "送信中" : "送っていません")
                        color: audio.micIndex === 0 ? Theme.mute : (meeting.sendingAudio && !returning.paused ? Theme.ok : Theme.muted)
                        font.family: app.sansFamily; font.pixelSize: 13; font.bold: audio.micIndex > 0
                    }
                }
                Text { text: "Zoomの声：このPCでは出しません"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
            }
        }
        Item { Layout.fillHeight: true }
    }

    Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.line }

    Flickable {
        Layout.fillWidth: false
        Layout.minimumWidth: 340
        Layout.preferredWidth: 340
        Layout.maximumWidth: 340
        Layout.fillHeight: true
        contentHeight: side.implicitHeight + 40
        clip: true

        ColumnLayout {
            id: side
            x: 20; y: 20
            width: parent.width - 40
            spacing: 16

            // ---- picture ----
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                z: 6
                Text { text: "映像の送り方"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
                Segmented {
                    firstText: "カメラとして"
                    secondText: "画面共有として"
                    first: !app.returnAsShare
                    onChosen: (first) => app.returnAsShare = !first
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: app.returnAsShare ? "共有画面として大きく見えます。入力のコマ数のまま、毎秒60コマまで送ります。ほかの人が共有している間は送れません。"
                                            : "参加者の1人の映像として見えます。毎秒30コマまでです（Zoomのカメラ映像の上限）。"
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                }
                Text { Layout.topMargin: 6; text: "映像の入力（USBキャプチャー）"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    z: videoPick.open ? 10 : 0
                    Dropdown {
                        id: videoPick
                        Layout.fillWidth: true
                        name: "映像の入力"
                        enabled: returning.available
                        options: returning.deviceNames
                        current: returning.deviceIndex
                        dimmed: returning.deviceIndex === 0
                        onPicked: (i) => returning.chooseDevice(i)
                    }
                    PlainButton {
                        visible: returning.available
                        text: "探し直す"
                        implicitHeight: 40
                        Accessible.name: "映像の機器を探し直す"
                        onClicked: returning.refreshDevices()
                    }
                }
                Text {
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: returning.problem.length > 0 ? returning.problem
                          : meeting.sendVideoNote.length > 0 ? meeting.sendVideoNote
                          : (!returning.available && !app.demo) ? "このPCには映像を取り込める機器がありません。"
                          : meeting.busy ? "機器と送り方の変更は、次に会議へ入るときから効きます。" : ""
                    color: returning.problem.length > 0 ? Theme.lost : meeting.sendVideoNote.length > 0 ? Theme.muted : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 12
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Zoomでの名前"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                        text: app.joinName
                        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

            // ---- sound ----
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                z: 5
                Text { text: "音声も送る"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
                ColumnLayout {
                    visible: app.audioAsio
                    Layout.fillWidth: true
                    spacing: 6
                    z: asioPick.open ? 10 : 0
                    Text { text: "ASIOの機器"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Dropdown {
                        id: asioPick
                        Layout.fillWidth: true
                        name: "ASIOの機器"
                        enabled: audio.available
                        options: audio.deviceNames
                        current: audio.deviceIndex
                        dimmed: audio.deviceIndex === 0
                        onPicked: (i) => audio.chooseDevice(i)
                    }
                }
                Text { text: "音声の入力"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    z: micPick.open ? 10 : 0
                    Dropdown {
                        id: micPick
                        Layout.fillWidth: true
                        name: "音声の入力"
                        enabled: audio.micAvailable
                        options: audio.micNames
                        current: audio.micIndex
                        dimmed: audio.micIndex === 0
                        onPicked: (i) => audio.chooseMic(i)
                    }
                    PlainButton {
                        visible: audio.micAvailable && !app.audioAsio
                        text: "探し直す"
                        implicitHeight: 40
                        Accessible.name: "音声の機器を探し直す"
                        onClicked: audio.refreshMics()
                    }
                }
                LevelBar { Layout.fillWidth: true; label: "L"; level: audio.micLevels[0] }
                LevelBar { Layout.fillWidth: true; label: "R"; level: audio.micLevels[1] }
                Text { text: "音質"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                Dropdown {
                    id: qualityPick
                    Layout.fillWidth: true
                    z: open ? 10 : 0
                    name: "Zoomへ送る音質"
                    options: ["会話向け（Zoomの標準の加工）", "原音（加工なし）", "原音・高音質（音楽向け）"]
                    current: app.sendQuality
                    onPicked: (i) => app.sendQuality = i
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: "チャンネル"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Segmented {
                        enabled: app.sendQuality > 0
                        opacity: enabled ? 1.0 : 0.4
                        firstText: "モノラル"
                        secondText: "ステレオ"
                        first: !app.sendStereo
                        onChosen: (first) => app.sendStereo = !first
                    }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: audio.micProblem.length > 0 ? audio.micProblem
                          : audio.problem.length > 0 && app.audioAsio ? audio.problem
                          : meeting.sendNote.length > 0 ? meeting.sendNote
                          : app.audioAsio && audio.micNames.length < 2 ? "ASIOの入力は、ASIOの機器を選ぶとここに並びます。WASAPIとの切り替えは「設定 → 音声出力」にあります。"
                          : "送るのは、演者の声を抜いた会場の音（マイナスワン）にしてください。「使わない」を選ぶと音は送りません。"
                    color: audio.micProblem.length > 0 || (audio.problem.length > 0 && app.audioAsio) ? Theme.lost
                           : meeting.sendNote.length > 0 ? Theme.muted : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 12
                }
            }

            PlainButton {
                Layout.fillWidth: true
                Layout.topMargin: 4
                implicitHeight: 40
                textSize: 13
                text: returning.paused ? "送信を再開する" : "送信を止める"
                tint: returning.paused ? Theme.ok : Theme.lost
                onClicked: returning.paused = !returning.paused
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "「送信を止める」は、会議に入ったまま映像と音だけを止めます。"
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
            }
        }
    }
}
