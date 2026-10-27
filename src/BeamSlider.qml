import QtQuick
import QtQuick.Controls
Slider {
    id: control
    implicitHeight: 24; leftPadding: 6; rightPadding: 6
    background: Rectangle {
        x: control.leftPadding; y: (control.height-height)/2
        width: control.availableWidth; height: 4; radius: 2
        color: Qt.alpha(theme.colors.foreground,.18)
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: Qt.alpha(theme.colors.foreground, control.enabled ? .8 : .25) }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition*(control.availableWidth-width)
        y: (control.height-height)/2; width: 12; height: 12; radius: 6
        color: control.enabled ? theme.colors.foreground : theme.colors.selection
        border.width: control.activeFocus ? 2 : 0; border.color: theme.accent
    }
}
