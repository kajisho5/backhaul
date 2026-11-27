import QtQuick
import QtQuick.Layouts
import Backhaul

// Project: the whole set-up of one event as a file.
ColumnLayout {
    id: root
    spacing: 0
    readonly property int textWidth: Math.min(width, 720)

    Text { text: "プロジェクト"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    Text {
        Layout.fillWidth: true
        Layout.maximumWidth: root.textWidth
        Layout.bottomMargin: 18
        wrapMode: Text.WordWrap
        text: "今の設定をまるごと1つのファイルに保存し、あとで開けます。案件ごと・会場ごとに作っておけます。"
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }

    RowLayout {
        Layout.bottomMargin: 14
        spacing: 12
        PlainButton {
            text: "ファイルに保存"
            strong: true
            textSize: 13
            implicitHeight: 40
            enabled: project.canPick
            onClicked: project.save()
        }
        PlainButton {
            text: "ファイルから開く"
            textSize: 13
            implicitHeight: 40
            color: Theme.control
            // Opening replaces the role and the devices, which are fixed while in a meeting.
            enabled: project.canPick && !meeting.busy
            onClicked: project.open()
        }
        Text {
            visible: project.fileName.length > 0
            text: project.fileName
            color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
        }
    }

    Text {
        visible: text.length > 0
        Layout.fillWidth: true
        Layout.maximumWidth: root.textWidth
        Layout.bottomMargin: 18
        wrapMode: Text.WordWrap
        text: project.message.length > 0 ? project.message
              : !project.canPick ? (app.demo ? "デモ表示では保存と読み込みはできません。" : "このPCではファイルを選ぶ窓を出せません。")
              : meeting.busy ? "会議に入っている間は、開くことはできません（保存はできます）。"
              : ""
        color: project.message.length > 0 ? (project.failed ? Theme.lost : Theme.ok) : Theme.mute
        font.family: app.sansFamily; font.pixelSize: 13
    }

    Rectangle { Layout.fillWidth: true; Layout.maximumWidth: root.textWidth; Layout.preferredHeight: 1; color: Theme.line; Layout.bottomMargin: 16 }

    Repeater {
        model: [
            { k: "入るもの", v: "役割、枠（名前・音量・遅延・出力先）、映像出力、音声の機器と音質、返し、プラグインとその中の設定、会議ID、表示名" },
            { k: "入らないもの", v: "Zoomの鍵（Client ID・Secret）、パスコード。これらはファイルにも設定にも書きません" },
            { k: "別のPCで開くとき", v: "機器やプラグインは名前と場所で探します。そのPCに無いものは、行に「見つかりません」と出て、設定は消さずに残します" }
        ]
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            Layout.maximumWidth: root.textWidth
            Layout.bottomMargin: 10
            spacing: 16
            Text {
                Layout.preferredWidth: 130
                Layout.alignment: Qt.AlignTop
                text: modelData.k
                color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: modelData.v
                color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
            }
        }
    }
}
