import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
ToolButton {
    property string symbol
    property string label
    icon.source: "qrc:/icons/" + symbol + ".svg"
    icon.width: 20; icon.height: 20
    icon.color: enabled ? highlighted ? theme.accent : "#eeeeef" : "#62626c"
    implicitWidth: 40; implicitHeight: 40
    Accessible.name: label
    ToolTip.visible: hovered
    ToolTip.text: label
    ToolTip.delay: 500
    Material.roundedScale: theme.radius
}
