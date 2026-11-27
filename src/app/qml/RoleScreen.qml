import QtQuick
import QtQuick.Layouts
import Backhaul

// Operator screen of the single-output roles (one person, two-shot):
// the outgoing picture large on the left, the few controls of the role on the right.
RowLayout {
    id: root
    readonly property bool twoShot: app.role === "twoshot"
    readonly property bool slide: app.role === "slide"
    spacing: 0

    ColumnLayout {
        id: main
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: 20
        spacing: 14

        // As large as the space allows, with the status line right below it.
        RoleTile {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(140, Math.min(main.height - statusBox.height - 14, (main.width - 4) * 9 / 16 + 48))
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
                    Text { text: "出力先"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                    Text {
                        text: app.roleOutputChoice > 0 ? app.roleOutputChoices[app.roleOutputChoice] : "未設定"
                        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                    }
                    Text {
                        text: app.roleOutputChoice > 0 ? "出力中" : "出していません"
                        color: app.roleOutputChoice > 0 ? Theme.ok : Theme.muted
                        font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                    }
                }
                Row {
                    spacing: 8
                    Text { text: "受け取り"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
                    Text {
                        text: root.slide ? "共有画面" : root.twoShot ? "2人・各720pまで" : (app.qualityAuto ? "1人・1080pまで" : "1人・720pまで")
                        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13
                    }
                }
                Text { text: "音声：このPCでは扱いません"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
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
            spacing: 18

            // ---- slide: whose screen ----
            ColumnLayout {
                visible: root.slide
                Layout.fillWidth: true
                spacing: 8
                Text { text: "スライド"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "だれかが画面共有を始めると、その画面を出します。共有する人が替わると、自動で新しい共有に切り替わります。"
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                }
            }

            // ---- one person: who ----
            ColumnLayout {
                visible: !root.twoShot && !root.slide
                Layout.fillWidth: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 4
                    Text { Layout.fillWidth: true; text: "出す人"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
                    Text { text: app.participantCount + "人"; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12 }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
                Text {
                    visible: app.participantCount === 0
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    wrapMode: Text.WordWrap
                    text: app.connected ? "まだだれもいません。" : "会議に参加すると、ここに参加者が並びます（設定 → Zoom接続）。"
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Repeater {
                    model: app.participants
                    delegate: Item {
                        id: person
                        required property int index
                        required property string name
                        required property string tag
                        required property bool inSlot
                        required property bool isSelf
                        // Anyone but this PC itself can be shown, hosts included (a chairperson often hosts).
                        readonly property bool canShow: !inSlot && !isSelf
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 116 - (roleTag.visible ? roleTag.width + 8 : 0)
                            text: person.name
                            elide: Text.ElideRight
                            color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13
                        }
                        Text {
                            visible: !person.canShow
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            text: person.inSlot ? "出力中" : person.tag
                            color: person.inSlot ? Theme.ok : Theme.mute
                            font.family: app.sansFamily; font.pixelSize: 12; font.bold: person.inSlot
                        }
                        Text {
                            id: roleTag
                            visible: person.canShow && (person.tag === "主催" || person.tag === "共同")
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: showButton.left
                            anchors.rightMargin: 8
                            text: person.tag
                            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11
                        }
                        PlainButton {
                            id: showButton
                            visible: person.canShow
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            text: "この人を出す"
                            strong: true
                            implicitHeight: 28
                            Accessible.name: person.name + "を出す"
                            onClicked: app.assignTo(0, person.index)
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
                    }
                }
            }

            // ---- two-shot: who is left and right ----
            ColumnLayout {
                visible: root.twoShot
                Layout.fillWidth: true
                spacing: 8
                z: 3
                Text { text: "並べる2人"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
                Text { text: "左"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                Dropdown {
                    Layout.fillWidth: true
                    z: open ? 20 : 0
                    name: "左に出す人"
                    options: app.pickNames
                    current: app.twoShotLeftPick
                    dimmed: current === 0
                    onPicked: (i) => app.pickForSlot(app.twoShotLeft, i)
                }
                PlainButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    text: "⇅ 左右を入れ替える"
                    implicitHeight: 32
                    onClicked: app.swapTwoShot()
                }
                Text { text: "右"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                Dropdown {
                    Layout.fillWidth: true
                    z: open ? 20 : 0
                    name: "右に出す人"
                    options: app.pickNames
                    current: app.twoShotRightPick
                    dimmed: current === 0
                    onPicked: (i) => app.pickForSlot(app.twoShotRight, i)
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

            // ---- the output ----
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                z: 2
                Text { text: "この出力"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    z: 5
                    Text { text: "出力先のモニター"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Dropdown {
                        Layout.fillWidth: true
                        name: "出力先のモニター"
                        options: app.roleOutputChoices
                        current: app.roleOutputChoice
                        dimmed: current === 0
                        onPicked: (i) => app.setRoleOutputChoice(i)
                    }
                    Text {
                        visible: app.roleOutputChoices.length < 2 && !app.demo
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "出力に使えるモニターがつながっていません。"
                        color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 12
                    }
                }

                // Name on the name card, and what to show when the line drops: one set per person.
                Repeater {
                    model: app.slotList
                    delegate: ColumnLayout {
                        id: one
                        required property int index
                        required property string label
                        required property bool freezeOnLoss
                        readonly property string side: !root.twoShot ? "" : (index === app.twoShotLeft ? "左：" : "右：")
                        visible: root.twoShot ? index < 2 : index === app.roleRow
                        Layout.fillWidth: true
                        spacing: 10
                        LabeledInput {
                            visible: !root.slide
                            Layout.fillWidth: true
                            label: one.side + "名前（待機中・カメラオフ時に表示）"
                            value: one.label
                            maxLength: 40
                            onEdited: (text) => app.setLabel(one.index, text)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: root.slide ? "共有が止まったとき" : one.side + "回線切れ時"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                            Segmented {
                                firstText: root.slide ? "最後の画面で静止" : "静止"
                                secondText: root.slide ? "黒" : "名前カード"
                                first: one.freezeOnLoss
                                onChosen: (first) => app.setFreezeOnLoss(one.index, first)
                            }
                        }
                    }
                }

                LabeledInput {
                    visible: root.slide
                    Layout.fillWidth: true
                    label: "まだ共有がないときに出す文字（空にすると黒）"
                    value: app.standbyText
                    maxLength: 24
                    onEdited: (text) => app.standbyText = text
                }
                ColumnLayout {
                    visible: root.slide
                    Layout.fillWidth: true
                    spacing: 6
                    Text { text: "画面の形が16:9でないとき"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Segmented {
                        firstText: "全体を見せる（黒帯）"
                        secondText: "16:9に切る"
                        first: !app.cropSlides
                        onChosen: (first) => app.cropSlides = !first
                    }
                }
                ColumnLayout {
                    visible: root.twoShot
                    Layout.fillWidth: true
                    spacing: 6
                    Text { text: "並べ方"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Segmented {
                        firstText: "縦長に切って大きく"
                        secondText: "16:9のまま並べる"
                        first: app.twoShotFill
                        onChosen: (first) => app.twoShotFill = first
                    }
                }
                ColumnLayout {
                    visible: root.twoShot
                    Layout.fillWidth: true
                    spacing: 6
                    Text { text: "片方がカメラオフのとき"; color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12 }
                    Segmented {
                        firstText: "名前カード"
                        secondText: "もう1人を全画面"
                        first: !app.twoShotSolo
                        onChosen: (first) => app.twoShotSolo = !first
                    }
                }
            }
        }
    }
}
