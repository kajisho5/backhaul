import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQml
import Backhaul

Window {
    id: win
    // The all-in-one role: whether the return tile (not a slot) is the selected one.
    property bool returnPicked: false
    // What the return is doing: "off" | "paused" | "sending" | "noinput" | "idle".
    readonly property bool returnSending: app.demo ? app.returnVideoOk : (meeting.sendingVideo || meeting.sendingAudio)
    readonly property string returnState: !app.sendsVideo ? "off"
        : returning.paused ? "paused"
        : returnSending ? "sending"
        : (!app.demo && returning.deviceIndex === 0 && audio.micIndex === 0) ? "noinput" : "idle"
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
    // Set from the command line for pictures of the "coming back" strip.
    property bool recoverSample: false
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
            title: qsTr("backhaul 出力")
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

        // Something dropped out and is being brought back (the meeting, a device).
        RecoverStrip {
            sample: recoverSample
            visible: shown
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            onDeviceRequested: (page) => { settingsTab = page; settingsOpen = true }
        }

        // A newer version is available (never shown during a meeting).
        UpdateStrip {
            visible: updates.noticeVisible
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            onNotesRequested: { settingsTab = "software"; settingsOpen = true }
        }

        // A test signal is sounding: said on every screen, with the way to stop it.
        Rectangle {
            visible: audio.testing
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 36 : 0
            color: Theme.muted
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: qsTr("テスト音を出しています　出力 ") + audio.testChannelsText
                    color: Theme.bg; font.family: app.sansFamily; font.pixelSize: 13; font.bold: true
                }
                PlainButton {
                    text: qsTr("止める")
                    tint: Theme.bg
                    implicitHeight: 26
                    Accessible.name: qsTr("テスト音を止める")
                    onClicked: audio.stopTest()
                }
            }
        }

        RoleScreen {
            visible: app.role === "camera" || app.role === "twoshot" || app.role === "slide"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RoleReturn {
            visible: app.role === "return"
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            visible: app.role === "all" || app.role === "audio"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            RoleAudio {
                visible: app.role === "audio"
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Flickable {
                visible: app.role === "all"
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
                                quiet: win.returnPicked
                                onPicked: win.returnPicked = false
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
                            picked: win.returnPicked
                            state: win.returnState
                            onChosen: win.returnPicked = true
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
                DetailPanel { Layout.fillWidth: true; visible: !win.returnPicked }
                ReturnDetail {
                    Layout.fillWidth: true
                    visible: win.returnPicked
                    state: win.returnState
                    onSettingsRequested: { settingsTab = "return"; settingsOpen = true }
                }
                ParticipantList { Layout.fillWidth: true; Layout.fillHeight: true; slotOnShow: !win.returnPicked }
            }
        }
    }
}
