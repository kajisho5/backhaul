import QtQuick
import QtQuick.Layouts
import Backhaul

// "Coming back": one line per thing that dropped out and is being brought back without
// the operator (the meeting, the sound devices, the capture device). Under the top bar;
// each line goes away when its thing is back.
Rectangle {
    id: root
    signal deviceRequested(string page)
    // Set from the command line for pictures: shows two sample lines.
    property bool sample: false

    readonly property var lines: {
        var list = []
        if (sample) {
            list.push({ kind: "meeting", title: qsTr("会議に入り直しています"),
                        text: qsTr("回線が切れました。%1回目を %2秒後に試します").arg(2).arg(8), waiting: true })
            list.push({ kind: "audio", title: qsTr("音声の機器を探しています"),
                        text: qsTr("「%1」が見つかりません。つなぎ直すと自動で戻ります（%2秒）").arg("Sample audio device").arg(12), waiting: false })
            list.push({ kind: "spare", title: qsTr("予備の機器で出力しています"),
                        text: qsTr("「%1」が戻りました。「元の機器に戻す」で切り替えます（一瞬音が途切れます）").arg("Sample audio device"), waiting: true })
            return list
        }
        if (meeting.rejoining) {
            list.push({ kind: "meeting", title: qsTr("会議に入り直しています"),
                        text: meeting.rejoinWaitSeconds > 0
                              ? qsTr("回線が切れました。%1回目を %2秒後に試します").arg(meeting.rejoinAttempt).arg(meeting.rejoinWaitSeconds)
                              : qsTr("回線が切れました。%1回目を試しています").arg(meeting.rejoinAttempt),
                        waiting: meeting.rejoinWaitSeconds > 0 })
        }
        if (audio.outputSeeking) {
            list.push({ kind: "audio", title: qsTr("音声の機器を探しています"),
                        text: qsTr("「%1」が見つかりません。つなぎ直すと自動で戻ります（%2秒）").arg(app.audioDeviceName).arg(audio.outputSeekSeconds), waiting: false })
        }
        if (audio.onSpare) {
            list.push({ kind: "spare", title: qsTr("予備の機器で出力しています"),
                        text: audio.mainBack
                              ? qsTr("「%1」が戻りました。「元の機器に戻す」で切り替えます（一瞬音が途切れます）").arg(app.audioDeviceName)
                              : qsTr("「%1」が戻らなかったため、「%2」に切り替えました").arg(app.audioDeviceName).arg(app.spareDeviceName),
                        waiting: audio.mainBack })
        }
        if (audio.micSeeking) {
            list.push({ kind: "audio", title: qsTr("入力の機器を探しています"),
                        text: qsTr("「%1」が見つかりません。つなぎ直すと自動で戻ります（%2秒）").arg(app.micDeviceName).arg(audio.micSeekSeconds), waiting: false })
        }
        if (returning.seeking) {
            list.push({ kind: "return", title: qsTr("映像の機器を探しています"),
                        text: qsTr("「%1」が見つかりません。つなぎ直すと自動で戻ります（%2秒）").arg(app.videoDeviceName).arg(returning.seekSeconds), waiting: false })
        }
        return list
    }
    readonly property bool shown: lines.length > 0

    color: Theme.panel
    implicitHeight: lines.length * 44
    Accessible.role: Accessible.AlertMessage
    Accessible.name: lines.length > 0 ? lines[0].title : ""

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }

    Column {
        anchors.fill: parent
        Repeater {
            model: root.lines
            delegate: Item {
                id: line
                required property var modelData
                width: root.width
                height: 44
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12
                    // Coming back is orange; running on the spare is a state to know about, yellow.
                    Rectangle { implicitWidth: 8; implicitHeight: 8; color: line.modelData.kind === "spare" ? Theme.muted : Theme.unstable }
                    Text {
                        text: line.modelData.title
                        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: line.modelData.text
                        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                    }
                    PlainButton {
                        visible: line.modelData.kind === "meeting"
                        enabled: line.modelData.waiting
                        text: qsTr("今すぐ試す"); textSize: 13
                        onClicked: meeting.rejoinNow()
                    }
                    PlainButton {
                        visible: line.modelData.kind === "meeting"
                        enabled: !app.locked  // giving up is leaving the meeting
                        text: qsTr("やめる"); textSize: 13
                        onClicked: meeting.leave()
                    }
                    PlainButton {
                        visible: line.modelData.kind === "spare"
                        enabled: line.modelData.waiting  // once the main device is there again
                        text: qsTr("元の機器に戻す"); textSize: 13
                        onClicked: audio.returnToMain()
                    }
                    PlainButton {
                        visible: line.modelData.kind !== "meeting" && line.modelData.kind !== "spare"
                        enabled: !app.locked
                        text: qsTr("機器を選び直す"); textSize: 13
                        onClicked: root.deviceRequested(line.modelData.kind)
                    }
                }
            }
        }
    }
}
