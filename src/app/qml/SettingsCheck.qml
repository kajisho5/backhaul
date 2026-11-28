import QtQuick
import QtQuick.Layouts
import Backhaul

// The check before the show: the points that stop shows, looked at in one go.
// Looking changes nothing.
ColumnLayout {
    id: root
    spacing: 0

    // A fresh look whenever the page is opened (the demo shows a sample result).
    onVisibleChanged: if (visible && !app.demo) check.run()
    Component.onCompleted: if (visible && !app.demo) check.run()

    RowLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: 860
        Text { text: qsTr("本番前チェック"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true }
        Item { Layout.fillWidth: true }
        PlainButton {
            text: check.running ? qsTr("確かめています…") : qsTr("もう一度確かめる")
            strong: true; textSize: 14; implicitHeight: 38
            enabled: !check.running && !app.demo
            onClicked: check.run()
        }
    }
    Text {
        Layout.topMargin: 8
        Layout.fillWidth: true
        Layout.maximumWidth: 860
        wrapMode: Text.WordWrap
        text: qsTr("本番の前に、止まりやすいところをまとめて確かめます。何も変更しません。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }
    Row {
        Layout.topMargin: 16
        Layout.bottomMargin: 12
        spacing: 16
        Accessible.role: Accessible.StaticText
        Accessible.name: qsTr("要対応 %1").arg(check.badCount) + "、" + qsTr("注意 %1").arg(check.warnCount) + "、" + qsTr("OK %1").arg(check.okCount)
        Text { visible: check.badCount > 0; text: qsTr("要対応 %1").arg(check.badCount); color: Theme.lost; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
        Text { visible: check.warnCount > 0; text: qsTr("注意 %1").arg(check.warnCount); color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
        Text { text: qsTr("OK %1").arg(check.okCount); color: Theme.ok; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
        Text {
            visible: check.checkedAt.length > 0
            anchors.baseline: parent.children[2].baseline
            text: qsTr("%1 に確認").arg(check.checkedAt)
            color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12
        }
    }
    Rectangle { Layout.fillWidth: true; Layout.maximumWidth: 860; implicitHeight: 1; color: Theme.line }

    Repeater {
        model: check.items
        delegate: Item {
            id: line
            required property var modelData
            readonly property string word: modelData.level === "ok" ? qsTr("OK") : modelData.level === "warn" ? qsTr("注意") : qsTr("要対応")
            Layout.fillWidth: true
            Layout.maximumWidth: 860
            implicitHeight: Math.max(52, detail.implicitHeight + 28)
            Accessible.role: Accessible.StaticText
            Accessible.name: modelData.name + "：" + word + "。" + modelData.text
            RowLayout {
                anchors.fill: parent
                spacing: 16
                Text {
                    Layout.preferredWidth: 184
                    text: line.modelData.name
                    color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 15; font.bold: true
                }
                Text {
                    id: detail
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: line.modelData.text
                    color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
                }
                Text {
                    text: line.word
                    color: line.modelData.level === "ok" ? Theme.ok : line.modelData.level === "warn" ? Theme.muted : Theme.lost
                    font.family: app.sansFamily; font.pixelSize: 14; font.bold: true
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
        }
    }
}
