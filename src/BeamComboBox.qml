import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
ComboBox {
    id: control
    implicitHeight: 28
    font.family: "monospace"; font.pixelSize: theme.fontSize
    leftPadding: 10; rightPadding: 28; topPadding: 0; bottomPadding: 0
    contentItem: Text {
        text: control.displayText; font: control.font; color: theme.colors.foreground
        opacity: control.enabled ? 1 : .4
        verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
    }
    indicator: Shape {
        x: control.width-22; y: (control.height-14)/2; width: 14; height: 14
        opacity: control.enabled ? 1 : .4
        ShapePath {
            strokeColor: theme.colors.foreground; strokeWidth: 1.2; fillColor: "transparent"
            startX: 3; startY: 5
            PathLine { x: 7; y: 9 }
            PathLine { x: 11; y: 5 }
        }
    }
    background: Rectangle {
        color: Qt.alpha(theme.colors.foreground, control.hovered || control.activeFocus ? .08 : .04)
        border.width: 1; border.color: Qt.alpha(control.activeFocus ? theme.accent : theme.colors.foreground, .4)
        radius: 0
    }
    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: control.width; height: 28
        contentItem: Text { text: modelData; font: control.font; color: theme.colors.foreground; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: Qt.alpha(theme.colors.foreground, parent.highlighted ? .18 : parent.hovered ? .08 : 0) }
        highlighted: control.highlightedIndex === index
    }
    popup: Popup {
        y: control.height+2; width: control.width; padding: 1
        implicitHeight: Math.min(contentItem.implicitHeight+2, 240)
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null; currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
        background: Rectangle { color: theme.colors.background; border.color: Qt.alpha(theme.colors.foreground,.4); border.width: 1 }
    }
}
