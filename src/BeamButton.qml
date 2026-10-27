import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
Button {
    id: control
    Material.roundedScale: theme.radius
    Material.foreground: highlighted ? theme.accentForeground : "#eeeeef"
    // Material's highlighted text ignores foreground, so choose readable ink explicitly.
    contentItem: Text {
        text: control.text
        font: control.font
        textFormat: Text.PlainText
        color: control.highlighted ? theme.accentForeground : "#eeeeef"
        opacity: control.enabled ? 1 : 0.38
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
