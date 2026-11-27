import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Backhaul

Window {
    width: 1440
    height: 900
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: "backhaul"
    color: Theme.bg

    // Set from the command line for pictures of the settings pages.
    property bool settingsOpen: false
    property string settingsTab: "slots"

    SettingsScreen {
        id: settings
        anchors.fill: parent
        visible: settingsOpen
        tab: settingsTab
        onTabChanged: settingsTab = tab
        onClosed: settingsOpen = false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: !settingsOpen

        TopBar { Layout.fillWidth: true; onSettingsRequested: settingsOpen = true }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentHeight: wall.implicitHeight + 32
                clip: true

                ColumnLayout {
                    id: wall
                    x: 16; y: 16
                    width: parent.width - 32
                    spacing: 14

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 14
                        rowSpacing: 14

                        Repeater {
                            model: app.slotList
                            delegate: SlotTile {
                                required property int gridRow
                                required property int gridColumn
                                Layout.row: gridRow
                                Layout.column: gridColumn
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1  // equal column widths
                                Layout.alignment: Qt.AlignTop
                            }
                        }
                        ReturnTile {
                            Layout.row: app.returnRow
                            Layout.column: 2
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                        }
                    }

                    RouteStrip { Layout.fillWidth: true }
                }
            }

            Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.line }

            ColumnLayout {
                Layout.fillWidth: false
                Layout.minimumWidth: 300
                Layout.preferredWidth: 300
                Layout.maximumWidth: 300
                Layout.fillHeight: true
                Layout.margins: 16
                spacing: 18
                DetailPanel { Layout.fillWidth: true }
                ParticipantList { Layout.fillWidth: true; Layout.fillHeight: true }
            }
        }
    }
}
