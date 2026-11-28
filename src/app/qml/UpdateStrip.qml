import QtQuick
import QtQuick.Layouts
import Backhaul

// "A newer version is available": a strip under the top bar. Shown only while this PC is
// not in a meeting; nothing is installed unless the operator presses the button.
Rectangle {
    id: root
    signal notesRequested()
    color: Theme.panel
    implicitHeight: 44
    Accessible.role: Accessible.AlertMessage
    Accessible.name: qsTr("バージョン %1 があります").arg(updates.latestVersion)

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Rectangle { implicitWidth: 8; implicitHeight: 8; color: Theme.ok }
        Text {
            text: qsTr("バージョン %1 があります").arg(updates.latestVersion)
            color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 14; font.bold: true
        }
        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: updates.selfUpdating
                  ? qsTr("今は %1 です。更新すると backhaul が再起動します（1分ほど）。").arg(updates.currentVersion)
                  : qsTr("今は %1 です。このビルドは自分では更新できません。").arg(updates.currentVersion)
            color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
        }
        PlainButton { text: qsTr("変更点"); textSize: 13; onClicked: root.notesRequested() }
        PlainButton { text: qsTr("あとで"); textSize: 13; onClicked: updates.later() }
        PlainButton {
            visible: updates.selfUpdating
            text: qsTr("今すぐ更新"); strong: true; textSize: 13
            enabled: updates.canInstall
            onClicked: updates.install()
        }
    }
}
