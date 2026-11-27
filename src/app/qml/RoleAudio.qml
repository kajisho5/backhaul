import QtQuick
import QtQuick.Layouts
import Backhaul

// Operator screen of the sound role: the device the meeting's voices go out through
// (to the desk), the device whose sound goes into the meeting (the desk's mix-minus),
// meters for both, and one line per person.
Flickable {
    id: root
    contentHeight: page.implicitHeight + 40
    clip: true

    ColumnLayout {
        id: page
        x: 20; y: 20
        width: root.width - 40
        spacing: 14

        GridLayout {
            Layout.fillWidth: true
            columns: page.width >= 760 ? 2 : 1
            columnSpacing: 14
            rowSpacing: 14
            z: 5

            // ---- speaker: the meeting's voices ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                implicitHeight: speaker.implicitHeight + 32
                color: Theme.panel
                border.width: 1
                border.color: Theme.line
                z: speakerPick.open ? 10 : 0

                ColumnLayout {
                    id: speaker
                    x: 18; y: 16
                    width: parent.width - 36
                    spacing: 12
                    // A test signal that would start from silence during a meeting waits here
                    // until the operator says yes. While one already sounds, more need no question.
                    property var waiting: null
                    function ask(run) {
                        if (app.connected && !audio.testing) waiting = run
                        else run()
                    }
                    Connections {
                        target: app
                        function onChanged() { if (!app.connected) speaker.waiting = null }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "スピーカー"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: "Zoomの声を出す機器（" + (app.audioAsio ? "ASIO" : "WASAPI") + "）"
                            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                        }
                        Text {
                            text: app.audioOk ? "出力中" : "止まっています"
                            color: app.audioOk ? Theme.ok : Theme.muted
                            font.family: app.sansFamily; font.pixelSize: 12; font.bold: true
                        }
                    }
                    Dropdown {
                        id: speakerPick
                        Layout.fillWidth: true
                        name: "スピーカーの機器"
                        enabled: audio.available
                        options: app.demo ? [app.audioDeviceName] : audio.deviceNames
                        current: app.demo ? 0 : audio.deviceIndex
                        dimmed: !app.demo && audio.deviceIndex === 0
                        onPicked: (i) => audio.chooseDevice(i)
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 18
                        rowSpacing: 8
                        Repeater {
                            model: audio.outputLevels
                            delegate: RowLayout {
                                id: channel
                                required property int index
                                required property var modelData
                                readonly property bool sounding: audio.testOn[index] === true
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                spacing: 8
                                LevelBar {
                                    Layout.fillWidth: true
                                    label: channel.index + 1
                                    level: channel.modelData
                                }
                                PlainButton {
                                    text: channel.sounding ? "停止" : "鳴らす"
                                    strong: channel.sounding
                                    tint: channel.sounding ? Theme.muted : Theme.ink
                                    implicitWidth: 58
                                    Accessible.name: "出力" + (channel.index + 1) + "のテスト音を" + (channel.sounding ? "止める" : "鳴らす")
                                    onClicked: speaker.ask(() => audio.toggleTest(channel.index))
                                }
                            }
                        }
                    }
                    // ---- sound check ----
                    Rectangle { visible: audio.outputLevels.length > 0; Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                    Flow {
                        visible: audio.outputLevels.length > 0
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            height: 32
                            verticalAlignment: Text.AlignVCenter
                            text: "サウンドチェック"
                            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                        }
                        Segmented {
                            firstText: "サイン 1kHz"
                            secondText: "ピンクノイズ"
                            first: audio.testKind === 0
                            onChosen: (first) => audio.testKind = first ? 0 : 1
                        }
                        Row {
                            height: 32
                            spacing: 6
                            PlainButton {
                                text: "−"; textSize: 15; implicitWidth: 32; implicitHeight: 32
                                enabled: audio.testLevelDb > audio.testLevelMin
                                Accessible.name: "テスト音を小さくする"
                                onClicked: audio.nudgeTestLevel(-1)
                            }
                            Text {
                                width: 58; height: 32
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                text: audio.testLevelDb + " dB"
                                color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
                            }
                            PlainButton {
                                text: "＋"; textSize: 15; implicitWidth: 32; implicitHeight: 32
                                enabled: audio.testLevelDb < audio.testLevelMax
                                Accessible.name: "テスト音を大きくする"
                                onClicked: audio.nudgeTestLevel(1)
                            }
                        }
                        PlainButton {
                            text: audio.testSweeping ? "順に鳴らしています" : "1から順に"
                            implicitHeight: 32
                            enabled: !audio.testSweeping
                            Accessible.name: "出力1から順にテスト音を鳴らす"
                            onClicked: speaker.ask(() => audio.sweepTest())
                        }
                        PlainButton {
                            text: "すべて止める"
                            implicitHeight: 32
                            enabled: audio.testing
                            tint: audio.testing ? Theme.muted : Theme.ink
                            onClicked: audio.stopTest()
                        }
                    }
                    // During a meeting the first press is asked about once: the venue would hear it.
                    Rectangle {
                        visible: speaker.waiting !== null
                        Layout.fillWidth: true
                        implicitHeight: confirm.implicitHeight + 20
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.muted
                        RowLayout {
                            id: confirm
                            x: 12; y: 10
                            width: parent.width - 24
                            spacing: 10
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: "会議に入っています。会場のスピーカーからテスト音が出ます（Zoomへは送りません）。"
                                color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 12
                            }
                            PlainButton {
                                text: "鳴らす"; strong: true; tint: Theme.muted
                                onClicked: { const run = speaker.waiting; speaker.waiting = null; run() }
                            }
                            PlainButton { text: "やめる"; onClicked: speaker.waiting = null }
                        }
                    }
                    Text {
                        visible: text.length > 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: audio.problem.length > 0 ? audio.problem
                              : (!audio.available && !app.demo) ? "このPCでは音を出せません。"
                              : "テスト音は選んだ出力にだけ出て、Zoomへは送りません。WASAPIとASIOの切り替えは「設定 → 音声出力」にあります。"
                        color: audio.problem.length > 0 ? Theme.lost : Theme.mute
                        font.family: app.sansFamily; font.pixelSize: 12
                    }
                }
            }

            // ---- microphone: the venue's sound into the meeting ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                implicitHeight: mic.implicitHeight + 32
                color: Theme.panel
                border.width: 1
                border.color: Theme.line
                z: micPick.open || qualityPick.open ? 10 : 0

                ColumnLayout {
                    id: mic
                    x: 18; y: 16
                    width: parent.width - 36
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "マイク"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: "Zoomへ送る機器"
                            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                        }
                        Text {
                            text: meeting.sendingAudio ? "送信中"
                                  : audio.micIndex > 0 ? (app.connected ? "まだ送れていません" : "取り込み中（会議の外）")
                                  : "使っていません"
                            color: meeting.sendingAudio ? Theme.ok : audio.micIndex > 0 ? Theme.muted : Theme.mute
                            font.family: app.sansFamily; font.pixelSize: 12; font.bold: true
                        }
                    }
                    RowLayout {
                      Layout.fillWidth: true
                      spacing: 8
                      z: micPick.open ? 10 : 0
                    Dropdown {
                        id: micPick
                        Layout.fillWidth: true
                        name: "マイクの機器"
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
                          Accessible.name: "マイクの機器を探し直す"
                          onClicked: audio.refreshMics()
                      }
                    }
                    LevelBar { Layout.fillWidth: true; label: "L"; level: audio.micLevels[0] }
                    LevelBar { Layout.fillWidth: true; label: "R"; level: audio.micLevels[1] }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        z: qualityPick.open ? 10 : 0
                        Text { text: "音質"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                        Dropdown {
                            id: qualityPick
                            Layout.fillWidth: true
                            name: "Zoomへ送る音質"
                            options: ["会話向け（Zoomの標準の加工）", "原音（加工なし）", "原音・高音質（音楽向け）"]
                            current: app.sendQuality
                            onPicked: (i) => app.sendQuality = i
                        }
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
                              : meeting.sendNote.length > 0 ? meeting.sendNote
                              : (!audio.micAvailable && !app.demo) ? "このPCには取り込める機器がありません。"
                              : app.audioAsio && audio.micNames.length < 2 ? "ASIOの入力は、スピーカー側の機器を選ぶとここに並びます。"
                              : meeting.busy ? "機器・音質・ステレオの変更は、次に会議へ入るときから効きます。"
                              : "卓で作ったマイナスワン（演者の声を抜いた音）をつないでください。ステレオは原音のときだけ選べます。"
                        color: audio.micProblem.length > 0 ? Theme.lost : meeting.sendNote.length > 0 ? Theme.muted : Theme.mute
                        font.family: app.sansFamily; font.pixelSize: 12
                    }
                }
            }
        }

        // ---- one line per person ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: table.implicitHeight + 16
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            ColumnLayout {
                id: table
                x: 18; y: 6
                width: parent.width - 36
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    spacing: 16
                    Text { Layout.preferredWidth: 22; text: "枠"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Text { Layout.fillWidth: true; Layout.preferredWidth: 3; text: "人"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Text { Layout.fillWidth: true; Layout.preferredWidth: 4; text: "レベル"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Text { Layout.preferredWidth: 76; text: "音量"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Text { Layout.preferredWidth: 76; text: "出力"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Item { Layout.preferredWidth: 72 }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

                Repeater {
                    model: app.slotList
                    delegate: Rectangle {
                        id: line
                        required property int index
                        required property string number
                        required property string label
                        required property string zoomName
                        required property string state
                        required property bool alsoMuted
                        required property bool isShare
                        required property double level
                        required property double gainDb
                        required property string channelText
                        readonly property bool chosen: app.selected === index
                        // Pictures do not matter here: only gone, muted or fine.
                        readonly property string soundState: state === "unassigned" ? "" : state === "lost" ? "lost"
                                                             : (state === "mute" || alsoMuted) ? "mute" : "ok"
                        visible: !isShare
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 52 : 0
                        color: chosen ? Theme.control : "transparent"
                        activeFocusOnTab: visible
                        Accessible.role: Accessible.Button
                        Accessible.name: "枠" + number + " " + label
                        Accessible.onPressAction: app.selected = index
                        Keys.onReturnPressed: app.selected = index
                        Keys.onSpacePressed: app.selected = index

                        RowLayout {
                            anchors.fill: parent
                            spacing: 16
                            Text { Layout.preferredWidth: 22; text: line.number; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13 }
                            Text {
                                Layout.fillWidth: true; Layout.preferredWidth: 3
                                elide: Text.ElideRight
                                text: line.zoomName.length > 0 ? line.zoomName : line.label + "（未割り当て）"
                                color: line.zoomName.length > 0 ? Theme.ink : Theme.mute
                                font.family: app.sansFamily; font.pixelSize: 14; font.bold: line.zoomName.length > 0
                            }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredWidth: 4
                                Layout.preferredHeight: 8
                                color: Theme.picture
                                border.width: 1
                                border.color: Theme.line
                                Rectangle {
                                    x: 1; y: 1
                                    height: parent.height - 2
                                    width: (parent.width - 2) * Math.max(0, Math.min(1, line.level))
                                    color: Theme.ok
                                }
                            }
                            Text {
                                Layout.preferredWidth: 76
                                text: (line.gainDb > 0 ? "+" : "") + line.gainDb.toFixed(1) + " dB"
                                color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
                            }
                            Text {
                                Layout.preferredWidth: 76
                                elide: Text.ElideRight
                                text: line.channelText.length > 0 ? line.channelText : "出さない"
                                color: line.channelText.length > 0 ? Theme.ink : Theme.mute
                                font.family: app.monoFamily; font.pixelSize: 13
                            }
                            Item {
                                Layout.preferredWidth: 72
                                Layout.preferredHeight: 22
                                Rectangle {
                                    visible: line.soundState.length > 0
                                    anchors.fill: parent
                                    color: line.soundState === "lost" ? Theme.lost : line.soundState === "mute" ? Theme.muted : Theme.ok
                                    Text {
                                        anchors.centerIn: parent
                                        text: line.soundState === "lost" ? "回線切れ" : line.soundState === "mute" ? "ミュート" : "正常"
                                        color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true
                                    }
                                }
                            }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                        MouseArea { anchors.fill: parent; onClicked: app.selected = line.index }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 42
            color: Theme.panel
            border.width: 1
            border.color: Theme.line
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 16
                width: parent.width - 32
                elide: Text.ElideRight
                text: "映像：このPCでは扱いません　／　出力チャンネルの割り当ては「設定 → 枠」で変えます"
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
            }
        }
    }
}
