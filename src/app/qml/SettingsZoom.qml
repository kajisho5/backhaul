import QtQuick
import QtQuick.Layouts
import Backhaul

// Zoom: the meeting to join, the app credentials, and where the connection stands.
ColumnLayout {
    id: root
    spacing: 0
    property bool editingKeys: false
    property bool editingLicence: false
    // The fold-outs start open when what is inside them is in use.
    property bool ownKeysOpen: meeting.credentialsSaved
    property bool manualZakOpen: meeting.zak.length > 0
    // The demo picture shows a PC that is set up (nothing is asked from any server).
    readonly property bool sample: app.demo
    readonly property bool lActive: sample || licence.active
    readonly property bool zSignedIn: sample || licence.zoomSignedIn
    readonly property bool wide: width >= 820
    // The licence and its Zoom sign-in are shared with other PCs: look again whenever this page is opened.
    onVisibleChanged: if (visible && !sample) licence.refresh()
    Component.onCompleted: if (visible && !sample) licence.refresh()

    Text { text: qsTr("Zoom接続"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

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
            text: app.demo ? qsTr("デモ表示では会議に参加しません。")
                           : qsTr("このプログラムには、Zoomにつなぐ部品が入っていません（Zoom Meeting SDKなしで作られたものです）。")
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
                Text { text: qsTr("会議"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    LabeledInput {
                        Layout.fillWidth: false
                        Layout.preferredWidth: 220
                        Layout.maximumWidth: 220
                        label: qsTr("会議ID")
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
                        label: qsTr("パスコード")
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
                    label: qsTr("参加するときの名前")
                    value: app.joinName
                    maxLength: 40
                    readOnly: meeting.busy
                    onEdited: (text) => app.joinName = text
                }
                RowLayout {
                    spacing: 12
                    PlainButton {
                        visible: !meeting.busy
                        text: qsTr("会議に入る")
                        strong: true
                        textSize: 14
                        implicitHeight: 44
                        enabled: meeting.canJoin
                        onClicked: meeting.join()
                    }
                    PlainButton {
                        visible: meeting.busy
                        text: qsTr("会議から出る")
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
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0 && meeting.busy
                    wrapMode: Text.WordWrap
                    text: meeting.joinNotice
                    color: Theme.muted
                    font.family: app.sansFamily; font.pixelSize: 13
                }
            }

            // Licence: the usual way to get into meetings without Zoom keys on this PC
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                Text { text: qsTr("ライセンス"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("購入時に届いたライセンスキーを入れると、会議に入るための鍵をこのアプリが自動で受け取ります。キーはこのPCの資格情報の保管場所に入れ、設定ファイルやプロジェクトには書きません。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Rectangle {
                    visible: root.lActive && !root.editingLicence
                    Layout.fillWidth: true
                    implicitHeight: 86
                    radius: 2
                    color: Theme.panel
                    border.width: 1
                    border.color: Theme.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            Text {
                                text: licence.messageBad ? qsTr("要確認") : qsTr("有効")
                                color: licence.messageBad ? Theme.muted : Theme.ok
                                font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: "BH-····-····-····-·" + (root.sample ? "7Q2M" : licence.keyTail)
                                color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
                            }
                            PlainButton { text: qsTr("入れ直す"); textSize: 13; implicitHeight: 32; enabled: !meeting.busy && !licence.busy; onClicked: root.editingLicence = true }
                            PlainButton { text: qsTr("このPCを外す"); textSize: 13; implicitHeight: 32; enabled: !meeting.busy && !licence.busy; onClicked: licence.deactivate() }
                        }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: root.sample ? qsTr("PC 2 / 5台　・　2027-11-30 まで　・　このPC：SAMPLE-PC-02")
                                  : "PC " + licence.used + " / " + licence.seats + qsTr("台　・　") + licence.expiresText + qsTr("　・　このPC：") + licence.deviceName
                            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                        }
                    }
                }
                RowLayout {
                    visible: licence.available && (!root.lActive || root.editingLicence)
                    Layout.fillWidth: true
                    spacing: 8
                    LabeledInput { id: keyInput; Layout.fillWidth: true; label: qsTr("ライセンスキー"); placeholder: "BH-XXXXX-XXXXX-XXXXX-XXXXX"; maxLength: 64; readOnly: licence.busy }
                    PlainButton {
                        Layout.alignment: Qt.AlignBottom
                        text: licence.busy ? qsTr("確かめています…") : qsTr("確かめて保存")
                        strong: true
                        textSize: 13
                        implicitHeight: 40
                        enabled: !licence.busy && keyInput.text.trim().length > 0
                        onClicked: licence.activate(keyInput.text)
                    }
                    PlainButton {
                        Layout.alignment: Qt.AlignBottom
                        visible: root.editingLicence
                        text: qsTr("やめる")
                        textSize: 13
                        implicitHeight: 40
                        onClicked: { keyInput.text = ""; root.editingLicence = false }
                    }
                }
                Connections {
                    target: licence
                    // A key that was taken closes the field; a refused one stays for correcting.
                    function onChanged() {
                        if (licence.active && !licence.busy && !licence.messageBad && keyInput.text.length > 0) {
                            keyInput.text = ""
                            root.editingLicence = false
                        }
                    }
                }
                Text {
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: root.sample ? "" : !licence.available ? qsTr("このPCではライセンスを保管できないため、使えません（Windowsで使ってください）。") : licence.message
                    color: licence.messageBad ? Theme.lost : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 13
                }
                Text {
                    text: qsTr("自分で作ったZoomアプリの鍵（Client ID・Secret）を使う　") + (root.ownKeysOpen ? "▾" : "▸")
                    color: ownKeysArea.containsMouse || activeFocus ? Theme.ink : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 12
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("自分で作ったZoomアプリの鍵を使う欄を") + (root.ownKeysOpen ? qsTr("閉じる") : qsTr("開く"))
                    Accessible.onPressAction: root.ownKeysOpen = !root.ownKeysOpen
                    Keys.onReturnPressed: root.ownKeysOpen = !root.ownKeysOpen
                    Keys.onSpacePressed: root.ownKeysOpen = !root.ownKeysOpen
                    MouseArea { id: ownKeysArea; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true; onClicked: root.ownKeysOpen = !root.ownKeysOpen }
                }
                Text {
                    visible: root.ownKeysOpen
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: meeting.credentialsSupported
                        ? qsTr("Zoomの開発者ページで自分で作ったアプリの鍵です。入っている間は、ライセンスよりこちらを使います。このPCの資格情報の保管場所に入れ、設定ファイルには書きません。")
                        : qsTr("このOSでは鍵を安全に保管できないため、保存できません（Windowsで使ってください）。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                ColumnLayout {
                    visible: root.ownKeysOpen
                    Layout.fillWidth: true
                    spacing: 14
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
                            Text { text: qsTr("保存済み"); color: Theme.ok; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true }
                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: "ID：" + meeting.credentialHint
                                color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 13
                            }
                            PlainButton { text: qsTr("入れ直す"); textSize: 13; implicitHeight: 36; enabled: !meeting.busy; onClicked: root.editingKeys = true }
                            PlainButton { text: qsTr("消す"); textSize: 13; implicitHeight: 36; enabled: !meeting.busy; onClicked: meeting.clearCredentials() }
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
                                text: qsTr("保存する")
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
                                text: qsTr("やめる")
                                textSize: 13
                                implicitHeight: 36
                                onClicked: {
                                    idInput.text = ""
                                    secretInput.text = ""
                                    root.editingKeys = false
                                }
                            }
                            Text { id: saveError; visible: false; text: qsTr("保存できませんでした"); color: Theme.lost; font.family: app.sansFamily; font.pixelSize: 13 }
                        }
                    }
                }
            }

            // The operator's Zoom account: what meetings of other accounts ask for
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 14
                Text { text: qsTr("Zoomアカウント"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("どれか1台でサインインすれば、同じライセンスのPCすべてに効きます。ほかのアカウントが開いた会議に入るための許可（ZAK）を、入るたびにアプリが自動で取ります。自分のアカウントの会議だけなら、サインインは要りません。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Rectangle {
                    visible: root.zSignedIn
                    Layout.fillWidth: true
                    implicitHeight: 86
                    radius: 2
                    color: Theme.panel
                    border.width: 1
                    border.color: Theme.line
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            Text { Layout.fillWidth: true; text: qsTr("サインイン済み"); color: Theme.ok; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true }
                            PlainButton { text: qsTr("サインアウト"); textSize: 13; implicitHeight: 32; enabled: !meeting.busy; onClicked: licence.signOut() }
                        }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: qsTr("このライセンスのPCすべてで有効です。サインアウトも全PCに効きます")
                            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 12
                        }
                    }
                }
                PlainButton {
                    visible: licence.available && !root.zSignedIn
                    text: licence.zoomWaiting ? qsTr("やめる") : qsTr("Zoomにサインイン")
                    strong: !licence.zoomWaiting
                    textSize: 13
                    implicitHeight: 40
                    enabled: root.lActive && !meeting.busy
                    onClicked: licence.zoomWaiting ? licence.cancelSignIn() : licence.signIn()
                }
                Text {
                    visible: text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: root.sample ? ""
                          : licence.zoomMessage.length > 0 ? licence.zoomMessage
                          : !licence.available || root.zSignedIn ? ""
                          : !root.lActive ? qsTr("サインインは、ライセンスキーを入れてからできます。")
                          : qsTr("ブラウザが開きます。サインインして「許可」を押すと、この画面に戻ります。")
                    color: licence.zoomMessageBad ? Theme.lost : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 12
                }
                Text {
                    text: qsTr("サインインできないときは、許可コード（ZAK）を手で貼る　") + (root.manualZakOpen ? "▾" : "▸")
                    color: zakArea.containsMouse || activeFocus ? Theme.ink : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 12
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("許可コードを手で貼る欄を") + (root.manualZakOpen ? qsTr("閉じる") : qsTr("開く"))
                    Accessible.onPressAction: root.manualZakOpen = !root.manualZakOpen
                    Keys.onReturnPressed: root.manualZakOpen = !root.manualZakOpen
                    Keys.onSpacePressed: root.manualZakOpen = !root.manualZakOpen
                    MouseArea { id: zakArea; anchors.fill: parent; anchors.margins: -6; hoverEnabled: true; onClicked: root.manualZakOpen = !root.manualZakOpen }
                }
                ColumnLayout {
                    visible: root.manualZakOpen
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("その日かぎりで使い、保存しません。サインイン済みのときは、自動で取ったものを優先します。")
                        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                    }
                    LabeledInput {
                        Layout.fillWidth: true
                        label: qsTr("許可コード（ZAK）")
                        value: meeting.zak
                        secret: true
                        maxLength: 2000
                        readOnly: meeting.busy
                        onEdited: (text) => meeting.zak = text
                    }
                }
            }

            // Picture size
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: qsTr("受け取る映像の大きさ"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Zoomの決まりで、同時に受け取れるのは 1080p なら1人、720p なら2人、360p なら4人までです。「自動」は映している人数に合わせて一番大きいものを選びます。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Segmented {
                    implicitHeight: 38
                    firstText: qsTr("自動")
                    secondText: qsTr("720pまで")
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
            Text { text: qsTr("つながるまでの流れ"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true; Layout.bottomMargin: 6 }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
            Repeater {
                model: [
                    { at: 1, title: qsTr("Zoomの鍵が通った"), note: qsTr("ライセンス（または自分の鍵）で作った入場券をZoomが確認すると進みます") },
                    { at: 2, title: qsTr("会議に入った"), note: meeting.step >= 2 ? app.meetingName : "" },
                    { at: 3, title: meeting.step === 2 ? qsTr("映像と音を受け取る許可を待っています") : qsTr("映像と音を受け取る許可が出た"),
                      note: meeting.step >= 3 ? "" : qsTr("ホストに、この参加者「") + app.joinName + qsTr("」を共同ホストにしてもらってください") },
                    { at: 3, title: qsTr("映像と音を受け取っている"), note: meeting.step >= 3 ? "" : qsTr("許可が出ると自動で始まります") }
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
