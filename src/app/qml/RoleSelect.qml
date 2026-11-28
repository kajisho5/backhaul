import QtQuick
import QtQuick.Layouts
import Backhaul

// "What is this PC for?" Shown on the first start and from the top bar.
Rectangle {
    id: root
    signal closed()
    color: Theme.bg

    readonly property var roles: [
        { key: "camera", title: qsTr("カメラ（1人）"), text: qsTr("1人の映像を、つないだモニターへ全画面で出す"), a: qsTr("受け取り：1人・最大1080p"), b: qsTr("音声：扱わない"), ready: true },
        { key: "twoshot", title: qsTr("2ショット"), text: qsTr("2人を左右に並べて、1枚の映像で出す"), a: qsTr("受け取り：2人・各720p"), b: qsTr("音声：扱わない"), ready: true },
        { key: "slide", title: qsTr("スライド"), text: qsTr("共有されている画面を全画面で出す"), a: qsTr("受け取り：共有画面"), b: qsTr("音声：扱わない"), ready: true },
        { key: "audio", title: qsTr("音声"), text: qsTr("Zoomの声を卓へ出し、卓のマイナスワンをZoomへ送る"), a: qsTr("映像：扱わない"), b: qsTr("音声：受けはこのPCだけ"), ready: true },
        { key: "return", title: qsTr("返し（映像・音声）"), text: qsTr("スイッチャーの出力をZoomへ送る。カメラとしても、画面共有としても送れる"), a: qsTr("送り：カメラ／画面共有"), b: qsTr("音声：送りだけ（選べる）"), ready: true },
        { key: "all", title: qsTr("1台でぜんぶ"), text: qsTr("枠を並べて、映像も音もこの1台で扱う"), a: qsTr("受け取り：人数に合わせて自動"), b: qsTr("音声：扱う"), ready: true }
    ]

    Rectangle {
        id: head
        width: parent.width
        height: 48
        color: Theme.bg
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            x: 16
            text: "backhaul"; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1
        }
        PlainButton {
            visible: app.role.length > 0
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 16
            text: qsTr("変えずに戻る")
            onClicked: root.closed()
        }
    }

    Flickable {
        anchors.top: head.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: page.implicitHeight + 88
        clip: true

        ColumnLayout {
            id: page
            x: 56; y: 44
            width: parent.width - 112
            spacing: 26

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: qsTr("このPCの役割"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 26; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("PCごとに1つ選びます。会議に入っていない間は、上のバーからいつでも変えられます。Zoomの声を会場へ出すPCは、会議の中で1台だけにしてください。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 14
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.width >= 860 ? 3 : 2
                columnSpacing: 16
                rowSpacing: 16

                Repeater {
                    model: root.roles
                    delegate: Rectangle {
                        id: card
                        required property int index
                        required property var modelData
                        readonly property bool current: app.role === modelData.key
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true   // cards in one row share the tallest card's height
                        Layout.minimumHeight: Math.max(190, body.implicitHeight + 42)
                        radius: 2
                        color: current || area.pressed ? Theme.panel : "transparent"
                        border.width: current || activeFocus ? 2 : 1
                        border.color: current || activeFocus ? Theme.ink : Theme.line
                        opacity: modelData.ready ? 1.0 : 0.45
                        activeFocusOnTab: modelData.ready

                        function choose() {
                            if (!modelData.ready) return
                            app.role = modelData.key
                            root.closed()
                        }
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title + "。" + modelData.text + (modelData.ready ? "" : qsTr("（準備中）"))
                        Accessible.onPressAction: choose()
                        Keys.onReturnPressed: choose()
                        Keys.onSpacePressed: choose()

                        ColumnLayout {
                            id: body
                            anchors.fill: parent
                            anchors.margins: 22
                            anchors.bottomMargin: 20
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Text { text: card.index + 1; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13 }
                                Text {
                                    Layout.fillWidth: true
                                    text: card.modelData.title
                                    elide: Text.ElideRight
                                    color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 20; font.bold: true
                                }
                                Text {
                                    visible: card.current || !card.modelData.ready
                                    text: card.modelData.ready ? qsTr("今の役割") : qsTr("準備中")
                                    color: card.modelData.ready ? Theme.ok : Theme.mute
                                    font.family: app.sansFamily; font.pixelSize: 11; font.bold: true
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                lineHeight: 1.25
                                text: card.modelData.text
                                color: "#c9cdd2"; font.family: app.sansFamily; font.pixelSize: 14
                            }
                            Item { Layout.fillHeight: true }
                            Text { text: card.modelData.a; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                            Text {
                                text: card.modelData.b
                                color: card.modelData.key === "audio" || card.modelData.key === "return" ? Theme.muted : Theme.mute
                                font.family: app.sansFamily; font.pixelSize: 12
                            }
                        }
                        MouseArea { id: area; anchors.fill: parent; enabled: card.modelData.ready; onClicked: card.choose() }
                    }
                }
            }
        }
    }
}
