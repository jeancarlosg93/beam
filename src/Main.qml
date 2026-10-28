import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 800; height: 580
    minimumWidth: 620; minimumHeight: 540
    visible: true; title: "Beam"
    color: theme.colors.background
    font.family: "monospace"; font.pixelSize: theme.fontSize
    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.accent: theme.accent; Material.primary: theme.colors.background
    Material.foreground: theme.colors.foreground; Material.roundedScale: theme.radius
    property bool allowClose: false
    readonly property color modalScrim: "#99000000"
    function titleAt(i) { return i >= 0 && i < backend.playlist.length ? backend.playlist[i].title : qsTr("No media selected") }
    function openPlaylist() { if (backend.dirty) discard.open(); else backend.choosePlaylist("open") }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.chooseFiles() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.togglePause() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; onActivated: backend.stop() }
    Shortcut { sequence: "B"; context: Qt.ApplicationShortcut; onActivated: backend.blackout = !backend.blackout }
    Shortcut { sequence: "Ctrl+Right"; context: Qt.ApplicationShortcut; onActivated: backend.next() }
    Shortcut { sequence: "Ctrl+Left"; context: Qt.ApplicationShortcut; onActivated: backend.previous() }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; onActivated: backend.seek(backend.position+5) }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; onActivated: backend.seek(backend.position-5) }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: backend.muted = !backend.muted }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: { backend.dismissIdentify(); backend.stop() } }
    onClosing: close => { if (backend.dirty && !allowClose) { close.accepted = false; quitConfirm.open() } }
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: "Beam"; font.pixelSize: theme.fontSize+4; font.weight: Font.DemiBold }
                Label { text: backend.outputState; color: backend.blackout ? theme.colors.red : theme.colors.light_foreground; Layout.fillWidth: true; leftPadding: 8; font.weight: backend.blackout ? Font.Bold : Font.Normal }
                BeamButton { text: qsTr("Blackout"); checkable: true; checked: backend.blackout; highlighted: checked; enabled: backend.projecting; onClicked: backend.blackout = !backend.blackout }
                BeamButton { text: qsTr("Preview"); checkable: true; checked: backend.previewEnabled; onClicked: { backend.previewEnabled = !backend.previewEnabled; if (backend.previewEnabled) win.height = Math.max(win.height,740) } }
                BeamButton { text: qsTr("Settings"); checkable: true; checked: settings.opened; onClicked: settings.opened ? settings.close() : settings.open() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: qsTr("Output"); font.weight: Font.DemiBold }
                BeamComboBox {
                    Layout.fillWidth: true; model: backend.screenLabels
                    currentIndex: backend.screens.indexOf(backend.screen); enabled: !backend.projecting
                    displayText: currentIndex < 0 ? qsTr("Unavailable: %1").arg(backend.screen) : currentText
                    Accessible.name: qsTr("Output display")
                    onActivated: backend.screen = backend.screens[currentIndex]
                }
                BeamButton { text: backend.projecting ? qsTr("End projection") : qsTr("Start projection"); highlighted: !backend.projecting; enabled: backend.screens.indexOf(backend.screen) >= 0; onClicked: backend.projecting ? backend.endProjection() : backend.project() }
            }
            Rectangle {
                visible: backend.error.length > 0; Layout.fillWidth: true; implicitHeight: errorRow.implicitHeight+12
                color: Qt.alpha(theme.colors.red,.1)
                RowLayout { id: errorRow; anchors.fill: parent; anchors.margins: 6
                    Label { text: backend.error; color: theme.colors.red; wrapMode: Text.WordWrap; Layout.fillWidth: true; maximumLineCount: 3; elide: Text.ElideRight }
                    BeamButton { flat: true; text: qsTr("Dismiss"); onClicked: backend.dismissError() }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: backend.playlistName + (backend.dirty ? " *" : ""); font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideMiddle }
                BeamButton { text: qsTr("Open…"); onClicked: win.openPlaylist() }
                BeamButton { text: qsTr("Save…"); onClicked: backend.choosePlaylist("save") }
                BeamButton { text: qsTr("Add media…"); onClicked: backend.chooseFiles() }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 80
                color: theme.colors.background; border.width: 1; border.color: Qt.alpha(theme.colors.foreground,.16)
                ListView {
                    id: queue; objectName: "queue"; anchors.fill: parent; anchors.margins: 1; clip: true
                    model: backend.playlist; currentIndex: backend.cueIndex
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: row; required property int index; required property var modelData
                        width: queue.width; height: 44
                        color: index === backend.cueIndex ? theme.colors.selection : rowHover.hovered ? Qt.alpha(theme.colors.foreground,.08) : "transparent"
                        HoverHandler { id: rowHover }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                            Item {
                                width: 20; Layout.fillHeight: true
                                Text { anchors.centerIn: parent; text: String(row.index+1); color: theme.colors.light_foreground; font: win.font }
                                MouseArea {
                                    id: drag; objectName: "reorder"+row.index; anchors.fill: parent; cursorShape: Qt.SizeVerCursor; preventStealing: true
                                    property real originY: 0
                                    onPressed: mouse => { originY = mapToItem(queue,0,mouse.y).y }
                                    onReleased: mouse => { const target = Math.max(0,Math.min(backend.playlist.length-1,Math.floor((mapToItem(queue,0,mouse.y).y+queue.contentY)/44))); backend.move(row.index,target) }
                                    Accessible.name: qsTr("Drag to reorder")
                                }
                            }
                            IconButton { symbol: "play"; label: qsTr("Play this video"); highlighted: backend.mediaActive && row.index === backend.current; onClicked: backend.play(row.index) }
                            Item {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                ColumnLayout {
                                    anchors.fill: parent; anchors.topMargin: 5; anchors.bottomMargin: 5; spacing: 2
                                    Label { text: row.modelData.title; elide: Text.ElideRight; Layout.fillWidth: true; font.weight: row.index === backend.current && backend.mediaActive ? Font.DemiBold : Font.Normal; color: row.modelData.missing ? theme.colors.red : theme.colors.foreground }
                                    Label { text: row.modelData.missing ? qsTr("File missing · Relink in settings") : row.modelData.path; elide: Text.ElideMiddle; Layout.fillWidth: true; color: theme.colors.light_foreground; font.pixelSize: theme.fontSize-1 }
                                }
                                MouseArea { objectName: "cueRow"+row.index; anchors.fill: parent; onClicked: backend.cue(row.index) }
                            }
                            Label { text: backend.clock(row.modelData.duration); color: theme.colors.light_foreground; font.pixelSize: theme.fontSize-1 }
                            IconButton { symbol: "up"; label: qsTr("Move up"); enabled: row.index > 0; onClicked: backend.move(row.index,row.index-1) }
                            IconButton { symbol: "down"; label: qsTr("Move down"); enabled: row.index+1 < backend.playlist.length; onClicked: backend.move(row.index,row.index+1) }
                            IconButton { symbol: "remove"; label: qsTr("Remove from playlist"); onClicked: backend.remove(row.index) }
                        }
                    }
                }
                Column {
                    anchors.centerIn: parent; spacing: 8; visible: backend.playlist.length === 0
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Your next screening starts here"); font.pixelSize: theme.fontSize+1 }
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Drop local videos here, or choose Add media."); color: theme.colors.light_foreground }
                }
                DropArea { anchors.fill: parent; onDropped: drop => { if (drop.hasUrls) { backend.addFiles(drop.urls); drop.acceptProposedAction() } } }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: backend.totalDuration; color: theme.colors.light_foreground; Layout.fillWidth: true; elide: Text.ElideRight }
                BeamCheckBox { text: qsTr("Auto next"); checked: backend.continuous; onToggled: backend.continuous = checked }
                BeamCheckBox { text: qsTr("Repeat"); checked: backend.repeat; onToggled: backend.repeat = checked }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Cue: %1").arg(win.titleAt(backend.cueIndex)); Layout.fillWidth: true; elide: Text.ElideRight; color: backend.cueItem.missing ? theme.colors.red : theme.colors.foreground }
                Label { text: qsTr("Start") }
                TextField {
                    objectName: "cueStart"; implicitWidth: 65; implicitHeight: 28; padding: 5
                    text: backend.cueItem.start !== undefined ? Number(backend.cueItem.start).toFixed(1) : "0.0"
                    color: theme.colors.foreground; font: win.font; selectByMouse: true
                    validator: DoubleValidator { bottom: 0; locale: "C" }
                    Accessible.name: qsTr("Cue start in seconds")
                    onEditingFinished: backend.setCueSetting("start",Number(text))
                    background: Rectangle { color: Qt.alpha(theme.colors.foreground,.04); border.color: Qt.alpha(theme.colors.foreground,.4) }
                }
                BeamButton { text: qsTr("Play cue"); highlighted: true; enabled: backend.cueIndex >= 0 && !backend.cueItem.missing; onClicked: backend.playCue() }
            }
            BeamSlider {
                Layout.fillWidth: true; from: 0; to: Math.max(1,backend.cueItem.duration || 0); value: backend.cueItem.start || 0
                enabled: backend.cueIndex >= 0 && backend.cueItem.duration > 0
                Accessible.name: qsTr("Private cue position"); onMoved: backend.setCueSetting("start",value)
            }
            RowLayout {
                visible: backend.previewEnabled; Layout.fillWidth: true; Layout.preferredHeight: visible ? 140 : 0; Layout.maximumHeight: 140; spacing: 8
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "black"
                    Image {
                        anchors.fill: parent; anchors.margins: 2; fillMode: Image.PreserveAspectFit; cache: false
                        source: !backend.projecting || backend.blackout ? "" : !backend.mediaActive && !backend.testing ? (backend.placeholder.length ? "file://"+backend.placeholder : "") : backend.livePreview
                    }
                    Label { anchors.top: parent.top; anchors.left: parent.left; padding: 5; text: qsTr("Output · %1").arg(backend.outputState); color: "white"; background: Rectangle { color: "#cc000000" } }
                    Label { anchors.centerIn: parent; width: parent.width-16; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: backend.projecting && backend.mediaActive && !backend.blackout && !backend.livePreview.length ? backend.previewMessage : ""; color: "white" }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "black"
                    Image { anchors.fill: parent; anchors.margins: 2; fillMode: Image.PreserveAspectFit; cache: false; source: backend.cuePreview }
                    Label { anchors.top: parent.top; anchors.left: parent.left; padding: 5; text: qsTr("Private cue · %1").arg(backend.clock(backend.cueItem.start || 0)); color: "white"; background: Rectangle { color: "#cc000000" } }
                    Label { anchors.centerIn: parent; visible: !backend.cuePreview.length; text: qsTr("Cue frame unavailable / loading"); color: "white"; width: parent.width-16; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.alpha(theme.colors.foreground,.12) }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Live: %1").arg(win.titleAt(backend.current)); font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                Label { text: backend.outputState; color: backend.blackout ? theme.colors.red : theme.colors.light_foreground }
                BeamComboBox {
                    model: ["0.5×","0.75×","1×","1.25×","1.5×","2×"]; property var speeds: [.5,.75,1,1.25,1.5,2]
                    currentIndex: speeds.indexOf(backend.speed); implicitWidth: 78; Accessible.name: qsTr("Playback speed"); onActivated: backend.speed = speeds[currentIndex]
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: backend.clock(timeline.pressed ? timeline.value : backend.position); color: theme.colors.light_foreground }
                BeamSlider { id: timeline; Layout.fillWidth: true; from: 0; to: Math.max(1,backend.duration); value: backend.position; enabled: backend.mediaActive && backend.duration > 0; Accessible.name: qsTr("Playback position"); onPressedChanged: if (!pressed) backend.seek(value) }
                Label { text: backend.clock(backend.duration-backend.position) + " / " + backend.clock(backend.duration); color: theme.colors.light_foreground }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                IconButton { symbol: "previous"; label: qsTr("Previous video"); enabled: backend.playlist.length > 0; onClicked: backend.previous() }
                IconButton { symbol: backend.playing ? "pause" : "play"; label: backend.playing ? qsTr("Pause") : qsTr("Play"); highlighted: true; enabled: backend.playlist.length > 0; onClicked: backend.togglePause() }
                IconButton { symbol: "next"; label: qsTr("Next video"); enabled: backend.playlist.length > 0; onClicked: backend.next() }
                IconButton { symbol: "stop"; label: qsTr("Stop to standby"); enabled: backend.mediaActive || backend.testing; onClicked: backend.stop() }
                Item { Layout.fillWidth: true }
                IconButton { symbol: backend.muted ? "muted" : "volume"; label: backend.muted ? qsTr("Unmute") : qsTr("Mute"); onClicked: backend.muted = !backend.muted }
                BeamSlider { from: 0; to: 100; value: backend.volume; Layout.preferredWidth: 100; Accessible.name: qsTr("Master volume"); onMoved: backend.volume = Math.round(value) }
                Label { text: (translator.language, backend.volume.toLocaleString(Qt.locale(),'f',0)) + "%"; Layout.preferredWidth: 42; color: theme.colors.light_foreground }
            }
        }
    }
    Drawer {
        id: settings; objectName: "settings"; edge: Qt.RightEdge; width: Math.min(440,win.width-40); height: win.height; padding: 16; leftPadding: 16; rightPadding: 16; topPadding: 16; bottomPadding: 16
        background: Rectangle { color: theme.colors.background; border.color: Qt.alpha(theme.colors.foreground,.4) }
        Overlay.modal: Rectangle { color: win.modalScrim }
        contentItem: ScrollView {
            id: settingsScroll; objectName: "settingsScroll"; clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: settingsScroll.availableWidth; spacing: 8
                RowLayout { Layout.fillWidth: true; Label { text: qsTr("Settings"); font.weight: Font.DemiBold; Layout.fillWidth: true } BeamButton { text: qsTr("Close"); onClicked: settings.close() } }
                Label { text: qsTr("Audio output"); font.weight: Font.DemiBold }
                BeamComboBox {
                    Layout.fillWidth: true; model: backend.audioDevices.map(d => d.label)
                    currentIndex: backend.audioDevices.map(d => d.id).indexOf(backend.audioDevice)
                    displayText: backend.audioDeviceAvailable ? currentText : qsTr("Unavailable: %1").arg(backend.audioDevice)
                    Accessible.name: qsTr("Audio output device"); onActivated: backend.audioDevice = backend.audioDevices[currentIndex].id
                }
                RowLayout { BeamButton { text: qsTr("Refresh devices"); onClicked: backend.refreshAudioDevices() } BeamButton { text: qsTr("System default"); onClicked: backend.audioDevice = "auto" } }
                BeamCheckBox { text: qsTr("Audio continues during blackout"); checked: backend.blackoutAudio; onToggled: backend.blackoutAudio = checked }
                Label { text: qsTr("Display setup"); font.weight: Font.DemiBold }
                RowLayout { BeamButton { text: qsTr("Identify displays"); onClicked: backend.identifyDisplays() } BeamButton { text: backend.testing ? qsTr("Return to standby") : qsTr("Test pattern"); enabled: backend.ready && !backend.mediaActive; onClicked: backend.testing ? backend.stop() : backend.testPattern() } }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: theme.colors.light_foreground; text: qsTr("Connect the intended display, choose its connector, then Start projection. Display loss closes output; reconnect and restart explicitly.") }
                Label { text: qsTr("Standby image"); font.weight: Font.DemiBold }
                Label { Layout.fillWidth: true; elide: Text.ElideMiddle; text: backend.placeholder.length ? backend.placeholder : qsTr("Black screen"); color: theme.colors.light_foreground }
                RowLayout { BeamButton { text: qsTr("Choose image…"); onClicked: backend.chooseFiles(true) } BeamButton { text: qsTr("Use black"); enabled: backend.placeholder.length > 0; onClicked: backend.clearPlaceholder() } }
                Label { text: qsTr("Playlist exchange"); font.weight: Font.DemiBold }
                RowLayout { BeamButton { text: qsTr("Import M3U…"); onClicked: win.openPlaylist() } BeamButton { text: qsTr("Export M3U…"); onClicked: backend.choosePlaylist("export") } BeamButton { text: qsTr("Shuffle"); onClicked: backend.shuffle() } }
                Label { text: qsTr("Private cue settings"); font.weight: Font.DemiBold }
                Label { text: win.titleAt(backend.cueIndex); Layout.fillWidth: true; elide: Text.ElideRight; color: backend.cueItem.missing ? theme.colors.red : theme.colors.light_foreground }
                Label { text: qsTr("Applied on the next Play cue. Live playback is unchanged."); Layout.fillWidth: true; wrapMode: Text.WordWrap; color: theme.colors.light_foreground }
                Label { text: qsTr("Audio track") }
                BeamComboBox {
                    Layout.fillWidth: true
                    property var tracks: [{id:"auto",label:qsTr("Automatic audio")},{id:"no",label:qsTr("Audio off")}].concat((backend.cueItem.tracks || []).filter(t => t.type === "audio"))
                    model: tracks.map(t => t.label); currentIndex: Math.max(0,tracks.map(t => t.id).indexOf(backend.cueItem.audio)); onActivated: backend.setCueSetting("audio",tracks[currentIndex].id)
                    Accessible.name: qsTr("Cue audio track")
                }
                Label { text: qsTr("Subtitle track") }
                BeamComboBox {
                    Layout.fillWidth: true
                    property var tracks: [{id:"no",label:qsTr("Subtitles off")},{id:"auto",label:qsTr("Automatic subtitles")}].concat((backend.cueItem.tracks || []).filter(t => t.type === "subtitle"))
                    model: tracks.map(t => t.label); currentIndex: Math.max(0,tracks.map(t => t.id).indexOf(backend.cueItem.subtitle)); onActivated: backend.setCueSetting("subtitle",tracks[currentIndex].id)
                    Accessible.name: qsTr("Cue subtitle track")
                }
                Label { text: qsTr("Fit / aspect") }
                BeamComboBox { Layout.fillWidth: true; model: [qsTr("Preserve original aspect"),qsTr("Fill / crop"),"4:3","16:9"]; property var values: ["preserve","fill","4:3","16:9"]; currentIndex: Math.max(0,values.indexOf(backend.cueItem.aspect)); onActivated: backend.setCueSetting("aspect",values[currentIndex]); Accessible.name: qsTr("Cue aspect") }
                BeamCheckBox { text: qsTr("Inherit master volume"); checked: backend.cueItem.volume === undefined || backend.cueItem.volume < 0; onToggled: backend.setCueSetting("volume",checked ? -1 : backend.volume) }
                RowLayout {
                    Layout.fillWidth: true
                    BeamSlider { Layout.fillWidth: true; from: 0; to: 100; value: Math.max(0,backend.cueItem.volume || 0); enabled: backend.cueItem.volume >= 0; onMoved: backend.setCueSetting("volume",Math.round(value)); Accessible.name: qsTr("Cue volume override") }
                    Label { text: backend.cueItem.volume >= 0 ? backend.cueItem.volume + "%" : qsTr("Master") }
                }
                RowLayout { BeamButton { text: qsTr("Reset cue settings"); enabled: backend.cueIndex >= 0; onClicked: backend.resetCueSettings() } BeamButton { text: qsTr("Relink cue…"); enabled: backend.cueIndex >= 0; onClicked: backend.choosePlaylist("relink") } }
                BeamButton { text: qsTr("Keyboard shortcuts"); onClicked: help.open() }
            }
        }
    }
    Popup {
        Overlay.modal: Rectangle { color: win.modalScrim }
        id: discard; anchors.centerIn: parent; width: Math.min(460,win.width-48); modal: true; focus: true; padding: 16
        background: Rectangle { color: theme.colors.background; border.color: Qt.alpha(theme.colors.foreground,.4) }
        ColumnLayout { anchors.fill: parent; Label { text: qsTr("Unsaved playlist changes"); font.weight: Font.DemiBold } Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Save first, or discard changes when opening another playlist.") } RowLayout { BeamButton { text: qsTr("Save…"); onClicked: { discard.close(); backend.choosePlaylist("save") } } BeamButton { text: qsTr("Discard and open…"); onClicked: { discard.close(); backend.choosePlaylist("open-discard") } } BeamButton { text: qsTr("Cancel"); onClicked: discard.close() } } }
    }
    Popup {
        Overlay.modal: Rectangle { color: win.modalScrim }
        id: quitConfirm; objectName: "quitConfirm"; anchors.centerIn: parent; width: Math.min(460,win.width-48); modal: true; focus: true; padding: 16
        background: Rectangle { color: theme.colors.background; border.color: Qt.alpha(theme.colors.foreground,.4) }
        ColumnLayout { anchors.fill: parent; Label { text: qsTr("Save named playlist before quitting?") } Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Session recovery is saved automatically. The named playlist has unsaved changes.") } RowLayout { BeamButton { text: qsTr("Save…"); onClicked: { quitConfirm.close(); backend.choosePlaylist("save") } } BeamButton { objectName: "quitWithoutSaving"; text: qsTr("Quit"); onClicked: { win.allowClose = true; quitConfirm.close(); win.close() } } BeamButton { text: qsTr("Cancel"); onClicked: quitConfirm.close() } } }
    }
    Popup {
        Overlay.modal: Rectangle { color: win.modalScrim }
        id: help; objectName: "shortcuts"; anchors.centerIn: parent; width: Math.min(500,win.width-48); modal: true; focus: true; padding: 16
        background: Rectangle { color: theme.colors.background; border.color: Qt.alpha(theme.colors.foreground,.4) }
        ColumnLayout { anchors.fill: parent; spacing: 8
            Label { text: qsTr("Keyboard shortcuts"); font.pixelSize: theme.fontSize+2 }
            Label { text: qsTr("Space    Play / pause\nB    Toggle blackout\nS / Esc    Stop to standby\nLeft / Right    Seek 5 seconds\nCtrl + Left / Right    Previous / next\nM    Mute / unmute\nCtrl + O    Add media\n?    Show shortcuts\nQ    Quit and close output"); lineHeight: 1.6 }
            BeamButton { text: qsTr("Close"); onClicked: help.close() }
        }
    }
}
