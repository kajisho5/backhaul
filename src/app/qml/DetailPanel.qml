import QtQuick
import QtQuick.Layouts
import Backhaul

// Settings of the selected slot. One copy per slot exists; only the selected one shows.
Item {
    implicitHeight: stack.implicitHeight

    Item {
        id: stack
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: childrenRect.height

        Repeater {
            model: app.slotList
            delegate: ColumnLayout {
                id: one
                required property int index
                required property string number
                required property string label
                required property string zoomName
                required property string sizeText
                required property string detailText
                required property string display
                required property string channelText
                required property double gainDb
                required property int delayMs
                required property bool freezeOnLoss
                required property bool isShare

                visible: app.selected === index
                width: stack.width
                spacing: 4

                Text { text: "選択中"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11 }
                Text {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 6
                    text: "枠 " + one.number + "　" + one.label
                    elide: Text.ElideRight
                    color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 20; font.bold: true
                }

                Repeater {
                    model: [
                        { k: "参加者", v: one.zoomName.length > 0 ? one.zoomName : "未割り当て", mono: false },
                        { k: "映像", v: one.sizeText + (one.detailText.length > 0 ? "・" + one.detailText : ""), mono: true },
                        { k: "出力先", v: [one.display, one.channelText].filter(t => t.length > 0).join(" ／ ") || "未設定", mono: true }
                    ]
                    delegate: Item {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        Text { anchors.verticalCenter: parent.verticalCenter; text: modelData.k; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: parent.width - 70
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

                Stepper {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    label: "音量"
                    valueText: (one.gainDb > 0 ? "+" : "") + one.gainDb.toFixed(1)
                    unit: "dB"
                    onStep: (direction) => app.nudgeGain(one.index, direction)
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; Layout.topMargin: 4; color: Theme.line }
                Stepper {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    label: "遅延"
                    valueText: one.delayMs.toString()
                    unit: "ms"
                    onStep: (direction) => app.nudgeDelay(one.index, direction)
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; Layout.topMargin: 4; color: Theme.line }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    Text { Layout.fillWidth: true; text: "回線切れ時"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                    Segmented {
                        firstText: "静止"
                        secondText: one.isShare ? "黒" : "名前カード"
                        first: one.freezeOnLoss
                        onChosen: (first) => app.setFreezeOnLoss(one.index, first)
                    }
                }
            }
        }
    }
}
