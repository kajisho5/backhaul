import QtQuick
import QtQuick.Layouts
import Backhaul

// Slots: who they are for and how they behave.
ColumnLayout {
    id: root
    spacing: 0

    Text { text: qsTr("枠"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    RowLayout {
        Layout.fillWidth: true
        spacing: 32

        // The list of slots
        ColumnLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: 220
            Layout.alignment: Qt.AlignTop
            spacing: 8
            Repeater {
                model: app.slotList
                delegate: Rectangle {
                    id: item
                    required property int index
                    required property string number
                    required property string label
                    readonly property bool on: app.selected === index
                    Layout.fillWidth: true
                    implicitHeight: 44
                    radius: 2
                    color: on ? Theme.control : "transparent"
                    border.width: 1
                    border.color: on || activeFocus ? Theme.ink : Theme.line
                    activeFocusOnTab: true
                    Accessible.role: Accessible.ListItem
                    Accessible.name: qsTr("枠") + number + " " + label
                    Keys.onReturnPressed: app.selected = index
                    Keys.onSpacePressed: app.selected = index
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12
                        Text { anchors.verticalCenter: parent.verticalCenter; width: 16; text: item.number; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 14 }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 28
                            elide: Text.ElideRight
                            text: item.label
                            color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: item.on
                        }
                    }
                    MouseArea { anchors.fill: parent; onClicked: app.selected = item.index }
                }
            }
            Rectangle {
                id: addButton
                Layout.fillWidth: true
                implicitHeight: 44
                radius: 2
                color: addArea.pressed ? Theme.control : "transparent"
                opacity: app.canAddSlot ? 1.0 : 0.4
                activeFocusOnTab: app.canAddSlot
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("枠を追加")
                Keys.onReturnPressed: app.addPersonSlot()
                Keys.onSpacePressed: app.addPersonSlot()
                // Dashed outline
                Canvas {
                    anchors.fill: parent
                    property color stroke: addButton.activeFocus ? Theme.ink : Theme.mute
                    onStrokeChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = stroke
                        ctx.lineWidth = 1
                        ctx.setLineDash([4, 3])
                        ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                    }
                }
                Text { anchors.centerIn: parent; text: qsTr("＋ 枠を追加"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14 }
                MouseArea { id: addArea; anchors.fill: parent; enabled: app.canAddSlot; onClicked: app.addPersonSlot() }
            }
        }

        // The selected slot
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 20

            Item {
                id: stack
                Layout.fillWidth: true
                implicitHeight: childrenRect.height

                Repeater {
                    model: app.slotList
                    delegate: GridLayout {
                        id: one
                        // Side by side when there is room, otherwise the sample goes below.
                        columns: stack.width >= 620 ? 2 : 1
                        columnSpacing: 24
                        rowSpacing: 20
                        required property int index
                        required property string label
                        required property string zoomName
                        required property bool freezeOnLoss
                        required property bool isShare
                        visible: app.selected === index
                        width: stack.width

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            spacing: 16
                            LabeledInput {
                                Layout.fillWidth: true
                                label: one.isShare ? qsTr("本番用の名前") : qsTr("本番用の名前（名前カードに出ます）")
                                value: one.label
                                maxLength: 40
                                onEdited: (text) => app.setLabel(one.index, text)
                            }
                            LabeledInput {
                                Layout.fillWidth: true
                                visible: !one.isShare
                                label: qsTr("Zoom上の名前（復帰したとき、この枠へ自動で戻します）")
                                value: one.zoomName
                                maxLength: 100
                                onEdited: (text) => app.setZoomName(one.index, text)
                            }
                            ColumnLayout {
                                spacing: 6
                                Text { text: qsTr("回線が切れたとき"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                                Segmented {
                                    implicitHeight: 38
                                    firstText: qsTr("最後の映像で静止")
                                    secondText: one.isShare ? qsTr("黒") : qsTr("名前カード")
                                    first: one.freezeOnLoss
                                    onChosen: (first) => app.setFreezeOnLoss(one.index, first)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            visible: !one.isShare
                            spacing: 6
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("名前カードの見本（カメラオフのとき出力に出ます）")
                                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                            }
                            Rectangle {
                                id: card
                                Layout.fillWidth: true
                                Layout.maximumWidth: 480
                                implicitHeight: width * 9 / 16
                                color: "#1c1f23"
                                border.width: 1
                                border.color: Theme.line
                                Column {
                                    anchors.centerIn: parent
                                    spacing: card.height * 0.045
                                    Rectangle {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: card.width * 0.22
                                        height: width
                                        radius: width / 2
                                        color: "#34383e"
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: Math.min(implicitWidth, card.width - 32)
                                        elide: Text.ElideRight
                                        text: one.label
                                        color: Theme.ink
                                        font.family: app.sansFamily
                                        font.pixelSize: Math.max(12, card.height * 0.1)
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: qsTr("スライドの出し方"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("共有が16:9でないとき（16:10など）の出し方です。切り取ると上下あわせて高さの10%が切れます。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Segmented {
                    implicitHeight: 38
                    firstText: qsTr("16:9に切り取る")
                    secondText: qsTr("全体を出して黒帯を付ける")
                    first: app.cropSlides
                    onChosen: (first) => app.cropSlides = first
                }
            }
        }
    }
}
