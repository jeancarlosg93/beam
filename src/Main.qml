import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 800; height: 580
    minimumWidth: 620; minimumHeight: 540
    visible: true
    title: "Beam"
    color: theme.colors.background
    font.family: "monospace"
    font.pixelSize: theme.fontSize
    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.primary: theme.colors.background
    Material.foreground: theme.colors.foreground
    Material.roundedScale: theme.radius
    property int selectedRow: backend.current
    function titleAt(i) { return i >= 0 && i < backend.playlist.length ? backend.playlist[i].title : qsTr("No media selected") }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.chooseFiles() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; onActivated: backend.stop() }
    Shortcut { sequence: "Ctrl+Right"; context: Qt.ApplicationShortcut; onActivated: backend.next() }
    Shortcut { sequence: "Ctrl+Left"; context: Qt.ApplicationShortcut; onActivated: backend.previous() }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; onActivated: backend.seek(backend.position+5) }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; onActivated: backend.seek(backend.position-5) }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: backend.muted = !backend.muted }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: backend.stop() }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Beam"; font.pixelSize: theme.fontSize+4; font.weight: Font.DemiBold }
                Label { text: qsTr("Video projection"); color: theme.colors.light_foreground; Layout.fillWidth: true; leftPadding: 8 }
                BeamButton { flat: true; text: qsTr("Shortcuts"); onClicked: help.open() }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.alpha(theme.colors.foreground,.12) }
            RowLayout {
                spacing: 8
                Label { text: qsTr("Output"); font.weight: Font.DemiBold }
                BeamComboBox {
                    id: output; Layout.fillWidth: true
                    Material.roundedScale: theme.radius
                    model: backend.screens
                    currentIndex: backend.screens.indexOf(backend.screen)
                    enabled: !backend.projecting
                    Accessible.name: qsTr("Output display")
                    onActivated: backend.screen = currentText
                }
                BeamButton {
                    text: backend.projecting ? qsTr("End projection") : qsTr("Start projection")
                    highlighted: !backend.projecting
                    enabled: backend.screens.length > 0
                    onClicked: backend.projecting ? backend.endProjection() : backend.project()
                }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: theme.colors.light_foreground; font.pixelSize: theme.fontSize
                text: backend.projecting ? (backend.ready ? qsTr("Fullscreen output on %1 · End projection to change displays").arg(backend.screen) : qsTr("Connecting to mpv…"))
                    : backend.screens.length < 2 ? qsTr("One display connected. Connect a second display to keep controls separate.") : qsTr("Controls stay here. Only media and standby appear on the output.")
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: qsTr("Standby"); color: theme.colors.light_foreground }
                Label {
                    Layout.fillWidth: true; elide: Text.ElideMiddle; color: theme.colors.foreground
                    text: backend.placeholder.length ? backend.placeholder.split('/').pop() : qsTr("Black screen")
                    ToolTip.visible: hover.hovered && backend.placeholder.length > 0
                    ToolTip.text: backend.placeholder
                    HoverHandler { id: hover }
                }
                BeamButton { text: qsTr("Choose image…"); onClicked: backend.chooseFiles(true) }
                BeamButton { flat: true; text: qsTr("Use black"); visible: backend.placeholder.length > 0; onClicked: backend.clearPlaceholder() }
            }
            Rectangle {
                visible: backend.error.length > 0; Layout.fillWidth: true
                implicitHeight: errorRow.implicitHeight+16; color: Qt.alpha(theme.colors.red,.1); radius: theme.radius
                RowLayout {
                    id: errorRow; anchors.fill: parent; anchors.margins: 8
                    Label { text: backend.error; color: theme.colors.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    BeamButton { flat: true; text: qsTr("Dismiss"); onClicked: backend.dismissError() }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Playlist"); font.pixelSize: theme.fontSize+1; font.weight: Font.DemiBold }
                Label { text: qsTr("%n videos", "", backend.playlist.length); color: theme.colors.light_foreground; Layout.fillWidth: true }
                BeamButton { flat: true; text: qsTr("Shuffle"); enabled: backend.playlist.length > 1; onClicked: backend.shuffle() }
                BeamButton { text: qsTr("Add media…"); onClicked: backend.chooseFiles() }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 100
                color: theme.colors.background; radius: 0
                border.width: 1; border.color: Qt.alpha(theme.colors.foreground,.16)
                ListView {
                    id: queue; anchors.fill: parent; anchors.margins: 1
                    clip: true; model: backend.playlist
                    currentIndex: win.selectedRow
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: row
                        required property int index
                        required property var modelData
                        width: queue.width; height: 44
                        color: index === backend.current ? theme.colors.selection : rowHover.hovered ? Qt.alpha(theme.colors.foreground,.08) : "transparent"
                        HoverHandler { id: rowHover }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 8
                            IconButton { symbol: "play"; label: qsTr("Play this video"); onClicked: backend.play(row.index) }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 3
                                Label { text: row.modelData.title; elide: Text.ElideRight; Layout.fillWidth: true; font.weight: row.index === backend.current ? Font.DemiBold : Font.Normal; color: row.modelData.missing ? theme.colors.red : theme.colors.foreground }
                                Label { text: row.modelData.missing ? qsTr("File missing") : row.modelData.path; elide: Text.ElideMiddle; Layout.fillWidth: true; color: row.index === backend.current ? theme.colors.foreground : theme.colors.light_foreground; font.pixelSize: theme.fontSize-1 }
                            }
                            IconButton { symbol: "up"; label: qsTr("Move up"); enabled: row.index > 0; onClicked: backend.move(row.index, row.index-1) }
                            IconButton { symbol: "down"; label: qsTr("Move down"); enabled: row.index+1 < backend.playlist.length; onClicked: backend.move(row.index, row.index+1) }
                            IconButton { symbol: "remove"; label: qsTr("Remove from playlist"); onClicked: backend.remove(row.index) }
                        }
                    }
                }
                Column {
                    anchors.centerIn: parent; spacing: 8; visible: backend.playlist.length === 0
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Your next screening starts here"); font.pixelSize: theme.fontSize+1 }
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Drop video files here, or choose Add media."); color: theme.colors.light_foreground }
                }
                DropArea { anchors.fill: parent; onDropped: drop => { if (drop.hasUrls) { backend.addFiles(drop.urls); drop.acceptProposedAction() } } }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                BeamCheckBox { text: qsTr("Continue automatically"); checked: backend.continuous; onToggled: backend.continuous = checked }
                Item { Layout.fillWidth: true }
                BeamCheckBox { text: qsTr("Repeat playlist"); checked: backend.repeat; onToggled: backend.repeat = checked }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.alpha(theme.colors.foreground,.12) }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 3
                    Label { text: win.titleAt(backend.current); font.pixelSize: theme.fontSize+2; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        color: backend.projecting ? theme.accent : theme.colors.light_foreground; font.pixelSize: theme.fontSize
                        text: !backend.projecting ? qsTr("Projection off") : !backend.ready ? qsTr("Connecting…") : !backend.mediaActive ? qsTr("Standby · %1").arg(backend.placeholder.length ? qsTr("Image") : qsTr("Black")) : backend.playing ? qsTr("Playing") : qsTr("Paused")
                    }
                }
                BeamComboBox {
                    id: speedBox; Material.roundedScale: theme.radius; model: ["0.5×", "0.75×", "1×", "1.25×", "1.5×", "2×"]
                    property var speeds: [0.5,0.75,1,1.25,1.5,2]
                    currentIndex: speeds.indexOf(backend.speed)
                    implicitWidth: 78; Accessible.name: qsTr("Playback speed")
                    onActivated: backend.speed = speeds[currentIndex]
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: backend.clock(timeline.pressed ? timeline.value : backend.position); color: theme.colors.light_foreground; font.family: "monospace" }
                BeamSlider {
                    id: timeline; Layout.fillWidth: true; from: 0; to: Math.max(1,backend.duration)
                    value: backend.position; enabled: backend.mediaActive && backend.duration > 0
                    Accessible.name: qsTr("Playback position")
                    onPressedChanged: if (!pressed) backend.seek(value)
                }
                Label { text: backend.clock(backend.duration); color: theme.colors.light_foreground; font.family: "monospace" }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                IconButton { symbol: "previous"; label: qsTr("Previous video"); enabled: backend.playlist.length > 0; onClicked: backend.previous() }
                IconButton { symbol: backend.playing ? "pause" : "play"; label: backend.playing ? qsTr("Pause") : qsTr("Play"); highlighted: true; enabled: backend.playlist.length > 0; onClicked: backend.togglePause() }
                IconButton { symbol: "next"; label: qsTr("Next video"); enabled: backend.playlist.length > 0; onClicked: backend.next() }
                IconButton { symbol: "stop"; label: qsTr("Stop to standby"); enabled: backend.mediaActive; onClicked: backend.stop() }
                Item { Layout.fillWidth: true }
                IconButton { symbol: backend.muted ? "muted" : "volume"; label: backend.muted ? qsTr("Unmute") : qsTr("Mute"); onClicked: backend.muted = !backend.muted }
                BeamSlider { from: 0; to: 100; value: backend.volume; Layout.preferredWidth: 100; Accessible.name: qsTr("Volume"); onMoved: backend.volume = Math.round(value) }
                Label { text: (translator.language, backend.volume.toLocaleString(Qt.locale(), 'f', 0)) + "%"; Layout.preferredWidth: 42; horizontalAlignment: Text.AlignRight; color: theme.colors.light_foreground }
            }
        }
    }
    Popup {
        id: help; anchors.centerIn: parent; width: Math.min(500,win.width-48); modal: true; focus: true; padding: 16
        background: Rectangle { color: theme.colors.background; border.width: 1; border.color: Qt.alpha(theme.colors.foreground,.4) }
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        ColumnLayout {
            anchors.fill: parent; spacing: 8
            Label { text: qsTr("Keyboard shortcuts"); font.pixelSize: theme.fontSize+2 }
            Label { text: qsTr("Space    Play / pause\nS / Esc    Stop to standby\nLeft / Right    Seek 5 seconds\nCtrl + Left / Right    Previous / next\nM    Mute / unmute\nCtrl + O    Add media\n?    Show shortcuts\nQ    Quit and close output"); lineHeight: 1.6 }
            BeamButton { text: qsTr("Close"); onClicked: help.close() }
        }
    }
}
