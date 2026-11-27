import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQml
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
    // The role question: always while no role is chosen, otherwise when asked for.
    property bool roleOpen: false
    readonly property bool roleAsked: !settingsOpen && (roleOpen || app.role.length === 0)

    RoleSelect {
        anchors.fill: parent
        visible: roleAsked
        onClosed: roleOpen = false
    }

    SettingsScreen {
        id: settings
        anchors.fill: parent
        visible: settingsOpen
        tab: settingsTab
        onTabChanged: settingsTab = tab
        onClosed: settingsOpen = false
        onIdentifyRequested: identifyTimer.restart()
    }

    // Full-screen output: one window on every monitor that has a slot.
    Instantiator {
        model: app.demo ? null : app.displays
        delegate: Window {
            required property int index
            required property int slot
            required property bool isOperator
            screen: Qt.application.screens[index]
            x: screen ? screen.virtualX : 0
            y: screen ? screen.virtualY : 0
            flags: Qt.Window | Qt.FramelessWindowHint
            color: "black"
            title: "backhaul 出力"
            visibility: slot >= 0 && !isOperator ? Window.FullScreen : Window.Hidden
            OutputView { anchors.fill: parent; visible: app.role !== "twoshot"; slotRow: slot }
            TwoShotView { anchors.fill: parent; visible: app.role === "twoshot" }
        }
    }

    // "Which monitor is which": a big number on every monitor for a few seconds.
    Timer { id: identifyTimer; interval: 4000 }
    Instantiator {
        model: Qt.application.screens
        delegate: Window {
            required property int index
            required property var modelData
            screen: modelData
            x: modelData.virtualX + 40
            y: modelData.virtualY + 40
            width: 300
            height: 220
            flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
            color: Theme.ink
            visible: identifyTimer.running
            Text {
                anchors.centerIn: parent
                text: index + 1
                color: Theme.bg
                font.family: app.monoFamily
                font.pixelSize: 150
                font.bold: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: !settingsOpen && !roleAsked

        TopBar { Layout.fillWidth: true; onSettingsRequested: settingsOpen = true; onRoleRequested: roleOpen = true }

        RoleScreen {
            visible: app.role === "camera" || app.role === "twoshot"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            visible: app.role === "all"
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
