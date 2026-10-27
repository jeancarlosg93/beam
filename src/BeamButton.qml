import QtQuick
import QtQuick.Controls
Button {
    id: control
    font.family: "monospace"
    font.pixelSize: theme.fontSize
    implicitHeight: 28
    implicitWidth: Math.max(28, contentItem.implicitWidth + 20)
    padding: 6; leftPadding: 10; rightPadding: 10
    topInset: 0; bottomInset: 0; leftInset: 0; rightInset: 0
    contentItem: Text {
        text: control.text; font: control.font; textFormat: Text.PlainText
        color: theme.colors.foreground
        opacity: control.enabled ? 1 : 0.4
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 0
        color: Qt.alpha(control.highlighted ? theme.accent : theme.colors.foreground,
            control.down ? .22 : control.hovered || control.activeFocus ? .08 : control.highlighted ? .12 : control.flat ? 0 : .04)
        border.width: control.flat && !control.activeFocus ? 0 : 1
        border.color: Qt.alpha(control.activeFocus || control.highlighted ? theme.accent : theme.colors.foreground, control.enabled ? .4 : .16)
    }
}
