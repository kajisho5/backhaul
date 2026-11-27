import QtQuick
import QtQuick.Layouts
import Backhaul

ColumnLayout {
    spacing: 0

    RowLayout {
        Layout.fillWidth: true
        Layout.bottomMargin: 4
        Text { Layout.fillWidth: true; text: "参加者"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true }
        Text { text: app.participantCount + "人"; color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12 }
    }
    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: app.participants
        delegate: Item {
            required property int index
            required property string name
            required property string tag
            required property bool canAssign
            width: ListView.view.width
            height: 40
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 90
                text: name
                elide: Text.ElideRight
                color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13
            }
            Text {
                visible: !canAssign
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                text: tag
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11
            }
            PlainButton {
                visible: canAssign
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                text: "この枠へ"
                strong: true
                implicitHeight: 28
                Accessible.name: name + "を選択中の枠へ割り当てる"
                onClicked: app.assignToSelected(index)
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
        }
    }
}
