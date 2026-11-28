import QtQuick
import QtQuick.Layouts
import Backhaul

// The plug-ins one stream of sound goes through, top to bottom.
// `target` is a slot row, or -1 for the sound that goes into the meeting.
ColumnLayout {
    id: root
    property int target: -1
    readonly property var rack: target < 0 ? plugins.micRack : (plugins.slotRacks[target] || [])
    readonly property real delayMs: target < 0 ? plugins.micDelayMs : (plugins.slotDelaysMs[target] || 0)
    readonly property bool full: rack.length >= plugins.maxPerRack
    property bool picking: false
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        Text { text: qsTr("プラグイン"); color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13 }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            visible: root.rack.length > 0
            text: root.delayMs > 0.05 ? qsTr("遅れ ＋") + root.delayMs.toFixed(1) + " ms" : qsTr("遅れなし")
            color: root.delayMs > 0.05 ? Theme.muted : Theme.mute
            font.family: app.monoFamily; font.pixelSize: 12
        }
    }

    Repeater {
        model: root.rack
        delegate: ColumnLayout {
            id: row
            required property int index
            required property var modelData
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    text: row.index + 1
                    color: Theme.mute; font.family: app.monoFamily; font.pixelSize: 12
                    Layout.preferredWidth: 12
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: row.modelData.name
                    color: row.modelData.loaded && !row.modelData.bypassed ? Theme.ink : Theme.mute
                    font.family: app.sansFamily; font.pixelSize: 13; font.bold: row.modelData.loaded && !row.modelData.bypassed
                }
                PlainButton {
                    visible: row.modelData.hasEditor
                    text: qsTr("画面")
                    implicitWidth: 44
                    Accessible.name: row.modelData.name + qsTr("の画面を開く")
                    onClicked: plugins.openEditor(root.target, row.index)
                }
                PlainButton {
                    visible: row.modelData.loaded
                    text: row.modelData.bypassed ? qsTr("切") : qsTr("入")
                    strong: !row.modelData.bypassed
                    tint: row.modelData.bypassed ? Theme.mute : Theme.ok
                    implicitWidth: 34
                    Accessible.name: row.modelData.name + (row.modelData.bypassed ? qsTr("を入れる（今はバイパス）") : qsTr("をバイパスする（今は入）"))
                    onClicked: plugins.setBypassed(root.target, row.index, !row.modelData.bypassed)
                }
                PlainButton {
                    visible: !row.modelData.loaded
                    text: qsTr("もう一度試す")
                    onClicked: plugins.retry(root.target, row.index)
                }
                PlainButton {
                    text: "×"
                    implicitWidth: 30
                    Accessible.name: row.modelData.name + qsTr("を外す")
                    onClicked: plugins.remove(root.target, row.index)
                }
            }
            Text {
                visible: row.modelData.note.length > 0
                Layout.fillWidth: true
                Layout.leftMargin: 18
                wrapMode: Text.WordWrap
                text: row.modelData.note
                color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 11
            }
        }
    }

    // A file that holds several effects: which one?
    ColumnLayout {
        visible: plugins.choiceTarget === root.target && plugins.choices.length > 0
        Layout.fillWidth: true
        spacing: 4
        Text { text: qsTr("このファイルには複数入っています。どれを使いますか"); color: Theme.muted; font.family: app.sansFamily; font.pixelSize: 12 }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(choiceColumn.height, 190) + 2
            color: Theme.picture; border.width: 1; border.color: Theme.line
            Flickable {
                anchors.fill: parent; anchors.margins: 1
                contentHeight: choiceColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: choiceColumn
                    width: parent.width
                    Repeater {
                        model: plugins.choices
                        delegate: PickLine { text: modelData; onChosen: plugins.choose(index) }
                    }
                }
            }
        }
        PlainButton { text: qsTr("やめる"); onClicked: plugins.cancelChoice() }
    }

    PlainButton {
        visible: plugins.available && !root.full
        Layout.fillWidth: true
        text: root.picking ? qsTr("閉じる") : qsTr("＋ プラグインを追加")
        onClicked: {
            if (!root.picking) plugins.refreshModules()
            root.picking = !root.picking
        }
    }
    Rectangle {
        visible: root.picking && !root.full && plugins.moduleNames.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(moduleColumn.height, 228) + 2
        color: Theme.picture; border.width: 1; border.color: Theme.line
        Flickable {
            anchors.fill: parent; anchors.margins: 1
            contentHeight: moduleColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: moduleColumn
                width: parent.width
                Repeater {
                    model: plugins.moduleNames
                    delegate: PickLine {
                        text: modelData
                        onChosen: {
                            root.picking = false
                            plugins.add(root.target, index)
                        }
                    }
                }
            }
        }
    }
    Text {
        visible: text.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: !plugins.available ? (app.demo ? "" : qsTr("このプログラムではプラグインを使えません。"))
              : root.picking && plugins.moduleNames.length === 0 ? qsTr("VST3プラグインが見つかりません（置き場所：C:\\Program Files\\Common Files\\VST3）")
              : root.full ? qsTr("1か所に入れられるのは") + plugins.maxPerRack + qsTr("個までです。")
              : ""
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 11
    }
    Text {
        visible: plugins.problem.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: plugins.problem
        color: Theme.lost; font.family: app.sansFamily; font.pixelSize: 11
    }
}
