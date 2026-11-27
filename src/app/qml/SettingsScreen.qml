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
        { key: "zoom", title: "Zoom接続", ready: true },
        { key: "slots", title: "枠", ready: true },
        { key: "video", title: "映像出力", ready: true },
        { key: "audio", title: "音声出力", ready: true },
        { key: "return", title: "返し", ready: false }
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
                Text { text: "設定"; color: Theme.ink; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: app.notice.length > 0 ? app.notice
                        : app.savesSettings ? "変更はすぐ反映され、自動で保存されます"
                        : "デモ表示のため、変更は保存されません"
                    color: app.notice.length > 0 ? Theme.lost : (app.savesSettings ? Theme.mute : Theme.muted)
                    font.family: app.sansFamily; font.pixelSize: 13
                }
                PlainButton { text: "操作画面へ戻る"; strong: true; textSize: 13; implicitHeight: 32; onClicked: root.closed() }
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
                        Accessible.name: modelData.title + (modelData.ready ? "" : "（準備中）")
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
                            Text {
                                visible: !navItem.modelData.ready
                                text: "準備中"
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
                contentHeight: (root.tab === "audio" ? audioPage.implicitHeight : root.tab === "video" ? videoPage.implicitHeight : root.tab === "zoom" ? zoomPage.implicitHeight : slotsPage.implicitHeight) + 56
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
