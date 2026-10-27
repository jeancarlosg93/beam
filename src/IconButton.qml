import QtQuick
import QtQuick.Controls
ToolButton {
    id: control
    property string symbol
    property string label
    icon.source: "qrc:/icons/" + symbol + ".svg"
    icon.width: 16; icon.height: 16
    icon.color: Qt.alpha(highlighted ? theme.accent : theme.colors.foreground, enabled ? 1 : .35)
    implicitWidth: 28; implicitHeight: 28
    padding: 5
    Accessible.name: label
    ToolTip.visible: hovered; ToolTip.text: label; ToolTip.delay: 500
    background: Rectangle {
        radius: 0; color: Qt.alpha(theme.colors.foreground, control.down ? .18 : control.hovered || control.activeFocus ? .08 : 0)
        border.width: control.activeFocus ? 1 : 0; border.color: theme.accent
    }
}
