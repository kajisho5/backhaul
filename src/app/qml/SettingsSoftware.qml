import QtQuick
import QtQuick.Layouts
import Backhaul

// Software: which version this PC has, and updating it.
ColumnLayout {
    id: root
    spacing: 0
    readonly property int textWidth: Math.min(width, 860)
    readonly property bool found: updates.status === "available"

    Text { text: qsTr("ソフトウェア"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    // A newer version was found.
    Rectangle {
        visible: root.found
        Layout.fillWidth: true
        Layout.maximumWidth: root.textWidth
        Layout.bottomMargin: 26
        implicitHeight: offer.implicitHeight + 36
        color: "transparent"
        border.width: 1
        border.color: Theme.ok
        RowLayout {
            id: offer
            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20; anchors.rightMargin: 20
            spacing: 20
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: qsTr("バージョン %1 があります").arg(updates.latestVersion)
                    color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 18; font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: !updates.selfUpdating ? qsTr("このビルドは自分では更新できません。新しいバージョンは、販売ページから入れ直してください。")
                          : meeting.busy ? qsTr("会議に入っている間は更新できません。")
                          : qsTr("更新すると backhaul が再起動します（1分ほど）。設定とライセンスはそのまま残ります。")
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
            }
            PlainButton {
                visible: updates.selfUpdating
                text: qsTr("今すぐ更新"); strong: true; textSize: 13; implicitHeight: 40
                enabled: updates.canInstall
                onClicked: updates.install()
            }
        }
    }

    Rectangle { Layout.fillWidth: true; Layout.maximumWidth: root.textWidth; Layout.preferredHeight: 1; color: Theme.line }
    Repeater {
        model: 3
        delegate: ColumnLayout {
            id: line
            required property int index
            Layout.fillWidth: true
            Layout.maximumWidth: root.textWidth
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12; Layout.bottomMargin: 12
                spacing: 14
                Text {
                    Layout.preferredWidth: 220
                    text: [qsTr("このPCのバージョン"), qsTr("最後に確かめた時刻"), qsTr("起動したときに確かめる")][line.index]
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 14
                }
                Text {
                    visible: line.index === 0
                    text: updates.currentVersion
                    color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 14
                }
                Text {
                    visible: line.index === 1
                    text: updates.status === "checking" ? qsTr("確かめています…")
                          : updates.status === "failed" ? qsTr("%1　確かめられませんでした。ネットを確かめてください").arg(updates.checkedAt)
                          : updates.status === "current" ? qsTr("%1　最新です").arg(updates.checkedAt)
                          : updates.status === "available" ? updates.checkedAt
                          : qsTr("まだ確かめていません")
                    color: updates.status === "failed" ? Theme.lost : Theme.ink
                    font.family: app.sansFamily; font.pixelSize: 14
                }
                PlainButton {
                    visible: line.index === 1
                    text: qsTr("今すぐ確かめる"); textSize: 13
                    enabled: updates.canCheck
                    onClicked: updates.checkNow()
                }
                Segmented {
                    visible: line.index === 2
                    implicitHeight: 32
                    Accessible.name: qsTr("起動したときに確かめる")
                    firstText: qsTr("入")
                    secondText: qsTr("切")
                    first: app.checksUpdates
                    onChosen: (first) => app.checksUpdates = first
                }
                Item { Layout.fillWidth: true }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }
        }
    }

    Text {
        visible: root.found
        Layout.topMargin: 28; Layout.bottomMargin: 8
        text: qsTr("バージョン %1 の変更点").arg(updates.latestVersion)
        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 16; font.bold: true
    }
    Text {
        visible: root.found
        Layout.fillWidth: true
        Layout.maximumWidth: Math.min(root.width, 640)
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        text: updates.notes
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 14
    }

    Text {
        Layout.topMargin: 28
        Layout.fillWidth: true
        Layout.maximumWidth: Math.min(root.width, 640)
        wrapMode: Text.WordWrap
        text: qsTr("backhaul が自分で更新を始めることはありません。会議に入っている間は、確かめることも更新することもしません。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }
}
