import QtQuick
import QtQuick.Controls
CheckBox {
    id: control
    font.family: "monospace"; font.pixelSize: theme.fontSize
    implicitHeight: 28; padding: 0; spacing: 8
    indicator: Rectangle {
        width: 14; height: 14; y: (control.height-height)/2; radius: 0
        color: Qt.alpha(theme.colors.foreground, control.hovered ? .08 : .04)
        border.width: 1; border.color: control.activeFocus ? theme.accent : Qt.alpha(theme.colors.foreground,.5)
        Rectangle { anchors.centerIn: parent; width: 8; height: 8; visible: control.checked; color: theme.colors.foreground }
    }
    contentItem: Text {
        text: control.text; font: control.font; leftPadding: 22
        color: theme.colors.foreground; verticalAlignment: Text.AlignVCenter
    }
}
