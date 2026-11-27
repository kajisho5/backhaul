import QtQuick
import QtQuick.Layouts
import Backhaul

// Video output: which slot each connected monitor shows full screen.
ColumnLayout {
    id: root
    signal identifyRequested()
    spacing: 0
    readonly property int tableWidth: Math.min(width, 860)

    Text { text: "映像出力"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    RowLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: root.tableWidth
        Layout.bottomMargin: 14
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Text { text: "モニターと枠"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "つながっているモニターごとに、全画面で出す枠を選びます。選んでいないモニターは黒を出します。"
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
            }
        }
        PlainButton {
            Layout.alignment: Qt.AlignBottom
            text: "各モニターに番号を出す"
            textSize: 13
            implicitHeight: 40
            color: Theme.control
            enabled: !app.demo
            onClicked: root.identifyRequested()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.maximumWidth: root.tableWidth
        Layout.bottomMargin: 14
        implicitHeight: notice.implicitHeight + 20
        radius: 2
        color: "transparent"
        border.width: 1
        border.color: Theme.muted
        Text {
            id: notice
            anchors.fill: parent
            anchors.margins: 10
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            wrapMode: Text.WordWrap
            text: "全画面への出力は、まだこのバージョンに入っていません。ここで選んだ割り当ては保存され、出力ができしだい使われます。"
            color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 13
        }
    }

    Text {
        visible: displayRows.count === 0
        text: "モニターが見つかりません"
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 14
    }

    Repeater {
        id: displayRows
        model: app.displays
        delegate: RowLayout {
            id: line
            required property int index
            required property int number
            required property string title
            required property string mode
            required property bool isOperator
            required property int slot
            required property string slotText
            Layout.fillWidth: true
            Layout.maximumWidth: root.tableWidth
            Layout.bottomMargin: 10
            spacing: 16
            z: choice.open ? 100 : 0

            Text {
                Layout.preferredWidth: 32
                text: line.number
                color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 20; font.bold: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 140
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: line.title
                    color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true
                }
                Text { text: line.mode; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12 }
            }
            Item {
                Layout.preferredWidth: 320
                Layout.minimumWidth: 200
                implicitHeight: 40
                // The operator's own monitor is never an output.
                Rectangle {
                    anchors.fill: parent
                    visible: line.isOperator
                    color: "transparent"
                    Canvas {
                        anchors.fill: parent
                        onWidthChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = Theme.line
                            ctx.lineWidth = 1
                            ctx.setLineDash([4, 3])
                            ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 12
                        width: parent.width - 24
                        elide: Text.ElideRight
                        text: line.slotText
                        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 14
                    }
                }
                Dropdown {
                    id: choice
                    anchors.fill: parent
                    visible: !line.isOperator
                    name: line.title
                    options: app.outputChoices
                    current: line.slot + 1
                    dimmed: line.slot < 0
                    onPicked: (index) => app.setDisplayChoice(line.index, index)
                }
            }
            Text {
                Layout.preferredWidth: 72
                horizontalAlignment: Text.AlignRight
                text: line.isOperator ? "—" : (line.slot >= 0 ? "割り当て済み" : "黒")
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12; font.bold: true
            }
        }
    }

    Rectangle { Layout.fillWidth: true; Layout.maximumWidth: root.tableWidth; Layout.topMargin: 18; Layout.bottomMargin: 22; implicitHeight: 1; color: Theme.line }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: 420
        spacing: 10
        z: gridChoice.open ? 100 : 0
        Text { text: "2×2まとめの出力先"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "操作画面で「2×2まとめ」にしたとき、4人を1枚にまとめて出すモニターです。"
            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
        }
        Dropdown {
            id: gridChoice
            Layout.preferredWidth: 320
            name: "2×2まとめの出力先"
            options: app.gridChoices
            current: app.gridChoice
            dimmed: app.gridChoice === 0
            onPicked: (index) => app.setGridChoice(index)
        }
        // Room for the open list above.
        Item { implicitHeight: 200 }
    }
}
