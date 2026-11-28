import QtQuick
import QtQuick.Layouts
import Backhaul

// Settings. Only the pages that are built are selectable.
Rectangle {
    id: root
    property string tab: "slots"
    signal closed()
    signal identifyRequested()
    color: Theme.bg

    readonly property var pages: [
        { key: "zoom", title: qsTr("Zoom接続"), ready: true },
        { key: "slots", title: qsTr("枠"), ready: true },
        { key: "video", title: qsTr("映像出力"), ready: true },
        { key: "audio", title: qsTr("音声出力"), ready: true },
        { key: "return", title: qsTr("返し"), ready: true },
        { key: "project", title: qsTr("プロジェクト"), ready: true },
        { key: "software", title: qsTr("ソフトウェア"), ready: true }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            implicitHeight: 48
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 18
                Text { text: "backhaul"; color: Theme.ink; font.family: app.monoFamily; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1 }
                Text { text: qsTr("設定"); color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: app.notice.length > 0 ? app.notice
                        : app.savesSettings ? qsTr("変更はすぐ反映され、自動で保存されます")
                        : qsTr("デモ表示のため、変更は保存されません")
                    color: app.notice.length > 0 ? Theme.lost : (app.savesSettings ? Theme.mute : Theme.muted)
                    font.family: app.sansFamily; font.pixelSize: 13
                }
                Segmented {
                    implicitHeight: 32
                    Accessible.name: "Language"
                    firstText: "English"
                    secondText: "日本語"
                    first: app.language === "en"
                    onChosen: (first) => app.language = first ? "en" : "ja"
                }
                PlainButton { text: qsTr("操作画面へ戻る"); strong: true; textSize: 13; implicitHeight: 32; onClicked: root.closed() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: false
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                Layout.margins: 16
                spacing: 4
                Repeater {
                    model: root.pages
                    delegate: Rectangle {
                        id: navItem
                        required property var modelData
                        readonly property bool on: root.tab === modelData.key
                        Layout.fillWidth: true
                        implicitHeight: 44
                        radius: 2
                        color: on ? Theme.ink : "transparent"
                        border.width: activeFocus && !on ? 1 : 0
                        border.color: Theme.ink
                        opacity: modelData.ready ? 1.0 : 0.4
                        activeFocusOnTab: modelData.ready
                        Accessible.role: Accessible.PageTab
                        Accessible.name: modelData.title + (modelData.ready ? "" : qsTr("（準備中）"))
                        Keys.onReturnPressed: if (modelData.ready) root.tab = modelData.key
                        Keys.onSpacePressed: if (modelData.ready) root.tab = modelData.key
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            Text {
                                Layout.fillWidth: true
                                text: navItem.modelData.title
                                color: navItem.on ? Theme.bg : Theme.ink
                                font.family: app.sansFamily; font.pixelSize: 14; font.bold: navItem.on
                            }
                            // A newer version is waiting on the "Software" page.
                            Rectangle {
                                visible: navItem.modelData.key === "software" && updates.status === "available"
                                implicitWidth: 8; implicitHeight: 8
                                color: navItem.on ? "#1b9e57" : Theme.ok
                            }
                            Text {
                                visible: !navItem.modelData.ready
                                text: qsTr("準備中")
                                color: Theme.ink
                                font.family: app.sansFamily; font.pixelSize: 11
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: navItem.modelData.ready
                            onClicked: root.tab = navItem.modelData.key
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.line }

            Flickable {
                id: scroller
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property real pageWidth: Math.max(width, 760)
                contentWidth: pageWidth
                contentHeight: (root.tab === "audio" ? audioPage.implicitHeight : root.tab === "video" ? videoPage.implicitHeight : root.tab === "zoom" ? zoomPage.implicitHeight : root.tab === "project" ? projectPage.implicitHeight : root.tab === "return" ? returnPage.implicitHeight : root.tab === "software" ? softwarePage.implicitHeight : slotsPage.implicitHeight) + 56
                boundsBehavior: Flickable.StopAtBounds

                SettingsAudio {
                    id: audioPage
                    visible: root.tab === "audio"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                }
                SettingsZoom {
                    id: zoomPage
                    visible: root.tab === "zoom"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                }
                SettingsVideo {
                    id: videoPage
                    visible: root.tab === "video"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                    onIdentifyRequested: root.identifyRequested()
                }
                SettingsReturn {
                    id: returnPage
                    visible: root.tab === "return"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                }
                SettingsProject {
                    id: projectPage
                    visible: root.tab === "project"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                }
                SettingsSoftware {
                    id: softwarePage
                    visible: root.tab === "software"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                }
                SettingsSlots {
                    id: slotsPage
                    visible: root.tab === "slots"
                    x: 32; y: 28
                    width: scroller.pageWidth - 64
                }
            }
        }
    }
}
