import QtQuick
import QtQuick.Layouts
import Backhaul

// Audio output: method, device, and which slot goes to which channel.
ColumnLayout {
    id: root
    spacing: 0
    readonly property int nameWidth: 150
    readonly property int numberWidth: 76
    readonly property int gap: 8

    Text { text: qsTr("音声出力"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    RowLayout {
        Layout.fillWidth: true
        Layout.bottomMargin: 28
        spacing: 24
        z: 50

        ColumnLayout {
            id: methodColumn
            spacing: 6
            Layout.alignment: Qt.AlignBottom
            Text { text: qsTr("方式"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Segmented {
                visible: audio.asioAvailable
                implicitHeight: 40
                firstText: "ASIO"
                secondText: "WASAPI"
                first: app.audioAsio
                onChosen: (first) => audio.setAsio(first)
            }
            Item {
                visible: !audio.asioAvailable
                implicitWidth: methodText.implicitWidth
                implicitHeight: 40
                Text {
                    id: methodText
                    anchors.verticalCenter: parent.verticalCenter
                    text: "WASAPI"
                    color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 14; font.bold: true
                }
            }
        }
        LabeledInput {
            visible: !audio.available
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            Layout.maximumWidth: 320
            Layout.alignment: Qt.AlignBottom
            label: qsTr("機器")
            value: app.audioDeviceName
            placeholder: qsTr("未選択")
            readOnly: true
        }
        ColumnLayout {
            visible: audio.available
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            Layout.maximumWidth: 320
            Layout.alignment: Qt.AlignBottom
            spacing: 6
            z: deviceChoice.open ? 100 : 0
            Text { text: qsTr("機器"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Dropdown {
                id: deviceChoice
                Layout.fillWidth: true
                name: qsTr("音声の機器")
                options: audio.deviceNames
                current: audio.deviceIndex
                dimmed: audio.deviceIndex === 0
                onPicked: (index) => audio.chooseDevice(index)
            }
        }
        PlainButton {
            visible: audio.available
            Layout.alignment: Qt.AlignBottom
            text: qsTr("探し直す")
            textSize: 13
            implicitHeight: 40
            onClicked: audio.refreshDevices()
        }
        Row {
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 11
            spacing: 12
            Text {
                text: (app.audioSampleRate / 1000) + qsTr("kHz・出力 ") + app.audioChannels + "ch"
                color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
            }
            Text {
                text: app.audioOk ? qsTr("正常") : qsTr("停止中")
                color: app.audioOk ? Theme.ok : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
        }
        Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
    }


    // The spare output device: takes over by itself when the main one stays away.
    RowLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: 860
        Layout.topMargin: -20
        Layout.bottomMargin: 8
        spacing: 24
        z: spareChoice.open ? 100 : 0
        // Under the main device's field, not under the method.
        Item { implicitWidth: methodColumn.width; implicitHeight: 1 }
        // Without a sound output on this system the saved name is only shown.
        LabeledInput {
            visible: !audio.available
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            Layout.maximumWidth: 320
            label: qsTr("予備の機器")
            value: app.spareDeviceName
            placeholder: qsTr("なし")
            readOnly: true
        }
        ColumnLayout {
            visible: audio.available
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            Layout.maximumWidth: 320
            spacing: 6
            Text { text: qsTr("予備の機器"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Dropdown {
                id: spareChoice
                Layout.fillWidth: true
                name: qsTr("予備の音声機器")
                options: audio.spareNames
                current: audio.spareIndex
                dimmed: audio.spareIndex === 0
                enabled: app.audioDeviceName.length > 0
                opacity: enabled ? 1.0 : 0.4
                onPicked: (index) => audio.chooseSpare(index)
            }
        }
        Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
    }
    Text {
        Layout.fillWidth: true
        Layout.maximumWidth: 860
        Layout.leftMargin: methodColumn.width + 24
        Layout.bottomMargin: 36
        wrapMode: Text.WordWrap
        text: qsTr("主の機器が3秒戻らないとき、予備へ自動で切り替えます。主の機器が戻っても、「元の機器に戻す」を押すまで予備のままです。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }

    Text {
        Layout.fillWidth: true
        Layout.topMargin: -16
        Layout.bottomMargin: 20
        wrapMode: Text.WordWrap
        text: audio.problem.length > 0 ? audio.problem
                : !audio.asioAvailable ? qsTr("このプログラムにはASIOが入っていません。Windowsの音声（WASAPI）で出します。")
                : app.audioAsio ? qsTr("ASIOは機器を48kHzで動かします。出力chの数は機器のドライバーに従います。")
                : qsTr("Windowsの音声（WASAPI）で出します。機器側の周波数へはWindowsが変換します。")
        color: audio.problem.length > 0 ? Theme.lost : Theme.mute
        font.family: app.sansFamily; font.pixelSize: 13
    }

    Text { text: qsTr("割り当て表"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true; Layout.bottomMargin: 6 }
    Text {
        Layout.fillWidth: true
        Layout.bottomMargin: 16
        wrapMode: Text.WordWrap
        text: qsTr("マスを押すと、その枠の音がそのchから出ます。同じchに複数の枠を入れると、まとめて出ます。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }

    // Column headings
    RowLayout {
        Layout.fillWidth: true
        Layout.bottomMargin: 8
        spacing: root.gap
        Text { Layout.preferredWidth: root.nameWidth; text: qsTr("枠"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
        Repeater {
            model: app.audioChannels
            delegate: Text {
                required property int index
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                horizontalAlignment: Text.AlignHCenter
                text: "ch " + (index + 1)
                color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12
            }
        }
        Text { Layout.preferredWidth: root.numberWidth; horizontalAlignment: Text.AlignHCenter; text: qsTr("音量"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
        Text { Layout.preferredWidth: root.numberWidth; horizontalAlignment: Text.AlignHCenter; text: qsTr("遅延"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
    }

    Repeater {
        model: app.slotList
        delegate: RowLayout {
            id: line
            required property int index
            required property string number
            required property string label
            required property int channels
            required property double gainDb
            required property int delayMs
            Layout.fillWidth: true
            Layout.bottomMargin: root.gap
            spacing: root.gap

            Row {
                Layout.preferredWidth: root.nameWidth
                spacing: 12
                Text { width: 14; text: line.number; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 14 }
                Text {
                    width: root.nameWidth - 26
                    elide: Text.ElideRight
                    text: line.label
                    color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true
                }
            }
            Repeater {
                model: app.audioChannels
                delegate: Rectangle {
                    id: cell
                    required property int index
                    readonly property bool on: (line.channels & (1 << index)) !== 0
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 44
                    radius: 2
                    color: on ? Theme.ok : "#0f1113"
                    border.width: activeFocus ? 2 : 1
                    border.color: activeFocus ? Theme.ink : (on ? Theme.ok : Theme.line)
                    activeFocusOnTab: true
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: qsTr("枠") + line.number + qsTr("をch ") + (index + 1) + qsTr("へ")
                    Accessible.checkable: true
                    Accessible.checked: on
                    Keys.onReturnPressed: app.toggleRoute(line.index, index + 1)
                    Keys.onSpacePressed: app.toggleRoute(line.index, index + 1)
                    Text {
                        anchors.centerIn: parent
                        visible: cell.on
                        text: qsTr("出力")
                        color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                    }
                    MouseArea { anchors.fill: parent; onClicked: app.toggleRoute(line.index, cell.index + 1) }
                }
            }
            Item {
                Layout.preferredWidth: root.numberWidth
                implicitHeight: 44
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Text { id: gainText; text: (line.gainDb > 0 ? "+" : "") + line.gainDb.toFixed(1); color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 14 }
                    Text { anchors.baseline: gainText.baseline; text: "dB"; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 11 }
                }
            }
            Item {
                Layout.preferredWidth: root.numberWidth
                implicitHeight: 44
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Text { id: delayText; text: line.delayMs; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 14 }
                    Text { anchors.baseline: delayText.baseline; text: "ms"; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 11 }
                }
            }
        }
    }
}
