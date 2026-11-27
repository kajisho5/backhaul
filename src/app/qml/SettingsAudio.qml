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

    Text { text: "音声出力"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    RowLayout {
        Layout.fillWidth: true
        Layout.bottomMargin: 28
        spacing: 24
        z: 50

        ColumnLayout {
            spacing: 6
            Layout.alignment: Qt.AlignBottom
            Text { text: "方式"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Item {
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
            label: "機器"
            value: app.audioDeviceName
            placeholder: "未選択"
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
            Text { text: "機器"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
            Dropdown {
                id: deviceChoice
                Layout.fillWidth: true
                name: "音声の機器"
                options: audio.deviceNames
                current: audio.deviceIndex
                dimmed: audio.deviceIndex === 0
                onPicked: (index) => audio.chooseDevice(index)
            }
        }
        PlainButton {
            visible: audio.available
            Layout.alignment: Qt.AlignBottom
            text: "探し直す"
            textSize: 13
            implicitHeight: 40
            onClicked: audio.refreshDevices()
        }
        Row {
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 11
            spacing: 12
            Text {
                text: (app.audioSampleRate / 1000) + "kHz・出力 " + app.audioChannels + "ch"
                color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
            }
            Text {
                text: app.audioOk ? "正常" : "停止中"
                color: app.audioOk ? Theme.ok : Theme.mute
                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
        }
        Item { Layout.fillWidth: true; Layout.preferredWidth: 0 }
    }

    Text {
        Layout.fillWidth: true
        Layout.topMargin: -16
        Layout.bottomMargin: 20
        wrapMode: Text.WordWrap
        text: audio.problem.length > 0 ? audio.problem : "ASIOはまだ使えません。Windowsの音声（WASAPI）で出します。"
        color: audio.problem.length > 0 ? Theme.lost : Theme.mute
        font.family: app.sansFamily; font.pixelSize: 13
    }

    Text { text: "割り当て表"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true; Layout.bottomMargin: 6 }
    Text {
        Layout.fillWidth: true
        Layout.bottomMargin: 16
        wrapMode: Text.WordWrap
        text: "マスを押すと、その枠の音がそのchから出ます。同じchに複数の枠を入れると、まとめて出ます。"
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }

    // Column headings
    RowLayout {
        Layout.fillWidth: true
        Layout.bottomMargin: 8
        spacing: root.gap
        Text { Layout.preferredWidth: root.nameWidth; text: "枠"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
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
        Text { Layout.preferredWidth: root.numberWidth; horizontalAlignment: Text.AlignHCenter; text: "音量"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
        Text { Layout.preferredWidth: root.numberWidth; horizontalAlignment: Text.AlignHCenter; text: "遅延"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
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
                    Accessible.name: "枠" + line.number + "をch " + (index + 1) + "へ"
                    Accessible.checkable: true
                    Accessible.checked: on
                    Keys.onReturnPressed: app.toggleRoute(line.index, index + 1)
                    Keys.onSpacePressed: app.toggleRoute(line.index, index + 1)
                    Text {
                        anchors.centerIn: parent
                        visible: cell.on
                        text: "出力"
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
