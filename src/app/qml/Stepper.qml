import QtQuick
import QtQuick.Layouts
import Backhaul

// "− value +" with large touch targets.
RowLayout {
    id: root
    property string label: ""
    property string valueText: ""
    property string unit: ""
    signal step(int direction)
    spacing: 10

    Text {
        text: root.label
        color: Theme.mute
        font.family: app.sansFamily
        font.pixelSize: 13
        Layout.preferredWidth: 40
    }
    PlainButton {
        text: "−"
        textSize: 18
        implicitWidth: 44
        implicitHeight: 44
        color: Theme.control
        Accessible.name: root.label + "を下げる"
        onClicked: root.step(-1)
    }
    Item {
        Layout.fillWidth: true
        implicitHeight: 44
        Row {
            anchors.centerIn: parent
            spacing: 6
            Text {
                id: valueLabel
                text: root.valueText
                color: Theme.ink
                font.family: app.monoFamily
                font.pixelSize: 28
                font.bold: true
            }
            Text {
                anchors.baseline: valueLabel.baseline
                text: root.unit
                color: Theme.mute
                font.family: app.monoFamily
                font.pixelSize: 12
            }
        }
    }
    PlainButton {
        text: "＋"
        textSize: 18
        implicitWidth: 44
        implicitHeight: 44
        color: Theme.control
        Accessible.name: root.label + "を上げる"
        onClicked: root.step(1)
    }
}
