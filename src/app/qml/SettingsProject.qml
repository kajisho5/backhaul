import QtQuick
import QtQuick.Layouts
import Backhaul

// Project: the whole set-up of one event as a file.
ColumnLayout {
    id: root
    spacing: 0
    readonly property int textWidth: Math.min(width, 720)

    Text { text: qsTr("プロジェクト"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 22; font.bold: true; Layout.bottomMargin: 20 }

    Text {
        Layout.fillWidth: true
        Layout.maximumWidth: root.textWidth
        Layout.bottomMargin: 18
        wrapMode: Text.WordWrap
        text: qsTr("今の設定をまるごと1つのファイルに保存し、あとで開けます。案件ごと・会場ごとに作っておけます。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }

    RowLayout {
        Layout.bottomMargin: 14
        spacing: 12
        PlainButton {
            text: qsTr("ファイルに保存")
            strong: true
            textSize: 13
            implicitHeight: 40
            enabled: project.canPick
            onClicked: project.save()
        }
        PlainButton {
            text: qsTr("ファイルから開く")
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
              : !project.canPick ? (app.demo ? qsTr("デモ表示では保存と読み込みはできません。") : qsTr("このPCではファイルを選ぶ窓を出せません。"))
              : meeting.busy ? qsTr("会議に入っている間は、開くことはできません（保存はできます）。")
              : ""
        color: project.message.length > 0 ? (project.failed ? Theme.lost : Theme.ok) : Theme.mute
        font.family: app.sansFamily; font.pixelSize: 13
    }

    Rectangle { Layout.fillWidth: true; Layout.maximumWidth: root.textWidth; Layout.preferredHeight: 1; color: Theme.line; Layout.bottomMargin: 16 }

    Repeater {
        model: [
            { k: qsTr("入るもの"), v: qsTr("役割、枠（名前・音量・遅延・出力先）、映像出力、音声の機器と音質、返し、プラグインとその中の設定、会議ID、表示名") },
            { k: qsTr("入らないもの"), v: qsTr("Zoomの鍵（Client ID・Secret）、パスコード。これらはファイルにも設定にも書きません") },
            { k: qsTr("別のPCで開くとき"), v: qsTr("機器やプラグインは名前と場所で探します。そのPCに無いものは、行に「見つかりません」と出て、設定は消さずに残します") }
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

    // The show log: what happened since the program was started, as a file.
    Text {
        Layout.topMargin: 28; Layout.bottomMargin: 8
        text: qsTr("本番ログ")
        color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 16; font.bold: true
    }
    Text {
        Layout.fillWidth: true
        Layout.maximumWidth: root.textWidth
        Layout.bottomMargin: 14
        wrapMode: Text.WordWrap
        text: qsTr("会議に入った・出た時刻、参加者の出入り、音声の機器が止まった時刻を記録しています。表計算ソフトで開けるCSVファイルに書き出せます。記録はこのPCの中だけにあり、backhaul を閉じると消えます。")
        color: Theme.mute; font.family: app.sansFamily; font.pixelSize: 13
    }
    RowLayout {
        Layout.bottomMargin: 10
        spacing: 12
        PlainButton {
            text: qsTr("CSVで書き出す")
            textSize: 13
            implicitHeight: 40
            color: Theme.control
            enabled: showLog.canSave && showLog.count > 0
            onClicked: showLog.save()
        }
        Text {
            text: qsTr("%1件").arg(showLog.count)
            color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13
        }
    }
    Text {
        visible: text.length > 0
        Layout.fillWidth: true
        Layout.maximumWidth: root.textWidth
        wrapMode: Text.WordWrap
        text: showLog.message.length > 0 ? showLog.message
              : !showLog.canSave ? (app.demo ? qsTr("デモ表示では書き出せません。") : qsTr("このPCではファイルを選ぶ窓を出せません。"))
              : ""
        color: showLog.message.length > 0 ? (showLog.failed ? Theme.lost : Theme.ok) : Theme.mute
        font.family: app.sansFamily; font.pixelSize: 13
    }
}
