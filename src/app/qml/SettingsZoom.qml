import QtQuick
import QtQuick.Layouts
import Backhaul

// Zoom: the meeting to join, the app credentials, and where the connection stands.
ColumnLayout {
    id: root
    spacing: 0
    property bool editingKeys: false
    readonly property bool wide: width >= 820

    Text { text: "Zoom接続"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    Rectangle {
        visible: !meeting.available
        Layout.fillWidth: true
        Layout.maximumWidth: 860
        Layout.bottomMargin: 20
        implicitHeight: missing.implicitHeight + 20
        radius: 2
        color: "transparent"
        border.width: 1
        border.color: Theme.muted
        Text {
            id: missing
            anchors.fill: parent
            anchors.margins: 10
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            wrapMode: Text.WordWrap
            text: app.demo ? "デモ表示では会議に参加しません。"
                           : "このプログラムには、Zoomにつなぐ部品が入っていません（Zoom Meeting SDKなしで作られたものです）。"
            color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 13
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: root.wide ? 2 : 1
        columnSpacing: 40
        rowSpacing: 28

        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 520
            Layout.alignment: Qt.AlignTop
            spacing: 28

            // The meeting
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                Text { text: "会議"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    LabeledInput {
                        Layout.fillWidth: false
                        Layout.preferredWidth: 220
                        Layout.maximumWidth: 220
                        label: "会議ID"
                        value: app.meetingId
                        placeholder: "123 4567 8901"
                        maxLength: 20
                        readOnly: meeting.busy
                        onEdited: (text) => app.meetingId = text
                    }
                    LabeledInput {
                        Layout.fillWidth: false
                        Layout.preferredWidth: 160
                        Layout.maximumWidth: 160
                        label: "パスコード"
                        value: meeting.passcode
                        secret: true
                        maxLength: 64
                        readOnly: meeting.busy
                        onEdited: (text) => meeting.passcode = text
                    }
                    Item { Layout.fillWidth: true }
                }
                LabeledInput {
                    Layout.fillWidth: true
                    label: "参加するときの名前"
                    value: app.joinName
                    maxLength: 40
                    readOnly: meeting.busy
                    onEdited: (text) => app.joinName = text
                }
                RowLayout {
                    spacing: 12
                    PlainButton {
                        visible: !meeting.busy
                        text: "会議に入る"
                        strong: true
                        textSize: 14
                        implicitHeight: 44
                        enabled: meeting.canJoin
                        onClicked: meeting.join()
                    }
                    PlainButton {
                        visible: meeting.busy
                        text: "会議から出る"
                        tint: Theme.lost
                        textSize: 14
                        implicitHeight: 44
                        onClicked: meeting.leave()
                    }
                    Text {
                        text: meeting.statusText
                        color: meeting.failed ? Theme.lost : (meeting.step >= 2 ? Theme.ok : Theme.mute)
                        font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                    }
                    Text { visible: meeting.busy && app.connected; text: app.elapsed; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13 }
                }
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    text: meeting.detail
                    color: meeting.failed ? Theme.lost : Theme.muted
                    font.family: app.sansFamily; font.pixelSize: 13
                }
            }

            // The app credentials
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                Text { text: "Zoomアプリの鍵"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: meeting.credentialsSupported
                        ? "Zoomの開発者ページで作ったアプリの Client ID と Client Secret です。このPCの資格情報の保管場所に入れ、設定ファイルには書きません。"
                        : "このOSでは鍵を安全に保管できないため、保存できません（Windowsで使ってください）。"
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Rectangle {
                    visible: meeting.credentialsSaved && !root.editingKeys
                    Layout.fillWidth: true
                    implicitHeight: 60
                    radius: 2
                    color: Theme.panel
                    border.width: 1
                    border.color: Theme.line
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 12
                        Text { text: "保存済み"; color: Theme.ok; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: "ID：" + meeting.credentialHint
                            color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13
                        }
                        PlainButton { text: "入れ直す"; textSize: 13; implicitHeight: 36; enabled: !meeting.busy; onClicked: root.editingKeys = true }
                        PlainButton { text: "消す"; textSize: 13; implicitHeight: 36; enabled: !meeting.busy; onClicked: meeting.clearCredentials() }
                    }
                }
                ColumnLayout {
                    visible: meeting.credentialsSupported && (!meeting.credentialsSaved || root.editingKeys)
                    Layout.fillWidth: true
                    spacing: 12
                    LabeledInput { id: idInput; Layout.fillWidth: true; label: "Client ID"; maxLength: 100 }
                    LabeledInput { id: secretInput; Layout.fillWidth: true; label: "Client Secret"; secret: true; maxLength: 200 }
                    RowLayout {
                        spacing: 8
                        PlainButton {
                            text: "保存する"
                            strong: true
                            textSize: 13
                            implicitHeight: 36
                            enabled: idInput.text.trim().length > 0 && secretInput.text.trim().length > 0
                            onClicked: {
                                if (meeting.saveCredentials(idInput.text, secretInput.text)) {
                                    idInput.text = ""
                                    secretInput.text = ""
                                    root.editingKeys = false
                                    saveError.visible = false
                                } else {
                                    saveError.visible = true
                                }
                            }
                        }
                        PlainButton {
                            visible: meeting.credentialsSaved
                            text: "やめる"
                            textSize: 13
                            implicitHeight: 36
                            onClicked: {
                                idInput.text = ""
                                secretInput.text = ""
                                root.editingKeys = false
                            }
                        }
                        Text { id: saveError; visible: false; text: "保存できませんでした"; color: Theme.lost; font.family: app.sansFamily; font.pixelSize: 13 }
                    }
                }
            }

            // Meetings of other accounts
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "ほかのアカウントの会議に入るとき"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "自分のZoomアカウント以外が開いた会議では、入るための許可コード（ZAK）が必要です。その日かぎりで使い、保存しません。"
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                LabeledInput {
                    Layout.fillWidth: true
                    label: "許可コード（ZAK）"
                    value: meeting.zak
                    secret: true
                    maxLength: 2000
                    readOnly: meeting.busy
                    onEdited: (text) => meeting.zak = text
                }
            }

            // Picture size
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "受け取る映像の大きさ"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Zoomの決まりで、同時に受け取れるのは 1080p なら1人、720p なら2人、360p なら4人までです。「自動」は映している人数に合わせて一番大きいものを選びます。"
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Segmented {
                    implicitHeight: 38
                    firstText: "自動"
                    secondText: "720pまで"
                    first: app.qualityAuto
                    onChosen: (first) => app.qualityAuto = first
                }
            }
        }

        // Where the connection stands
        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 420
            Layout.alignment: Qt.AlignTop
            spacing: 0
            Text { text: "つながるまでの流れ"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true; Layout.bottomMargin: 6 }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            Repeater {
                model: [
                    { at: 1, title: "Zoomアプリの鍵が通った", note: "鍵が正しいことをZoomが確認すると進みます" },
                    { at: 2, title: "会議に入った", note: meeting.step >= 2 ? app.meetingName : "" },
                    { at: 3, title: meeting.step === 2 ? "映像と音を受け取る許可を待っています" : "映像と音を受け取る許可が出た",
                      note: "ホストに、この参加者「" + app.joinName + "」を共同ホストにしてもらってください" },
                    { at: 3, title: "映像と音を受け取っている", note: "許可が出ると自動で始まります" }
                ]
                delegate: ColumnLayout {
                    id: stepRow
                    required property var modelData
                    required property int index
                    readonly property bool done: meeting.step >= modelData.at
                    readonly property bool now: index === 2 && meeting.step === 2
                    Layout.fillWidth: true
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        Layout.bottomMargin: 12
                        spacing: 12
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            implicitWidth: 22
                            implicitHeight: 22
                            radius: 11
                            color: stepRow.done ? Theme.ok : "transparent"
                            border.width: 1
                            border.color: stepRow.done ? Theme.ok : (stepRow.now ? Theme.muted : Theme.line)
                            Text { anchors.centerIn: parent; visible: stepRow.done; text: "✓"; color: Theme.bg; font.pixelSize: 13; font.bold: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: stepRow.modelData.title
                                color: stepRow.now ? Theme.muted : Theme.ink
                                font.family: app.sansFamily; font.pixelSize: 14; font.bold: stepRow.now
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                wrapMode: Text.WordWrap
                                text: stepRow.modelData.note
                                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
                }
            }
        }
    }
}
