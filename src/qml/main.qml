import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import ScriptureRT 1.0

// Scripture for Android — the Windows/macOS overlay re-implemented on plain
// Qt Quick with a C++ backend. The window fills the screen; the verse overlay
// and the embedded settings panel (ScriptureSettings.qml) swap as full-screen
// layers. All behavior lives in the C++ `App` controller.

Window {
    id: overlay
    objectName: "overlayWindow"
    visible: App.overlayOpen || App.settingsOpen
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "#000000"

    onVisibleChanged: if (visible) {
        // Fill the screen geometry manually. A frameless always-on-top window
        // does not necessarily become the key window by itself, and without
        // key status the Esc handler never fires — ask for activation.
        const scr = overlay.screen
        if (scr) {
            overlay.x = scr.virtualX
            overlay.y = scr.virtualY
            overlay.width = scr.width
            overlay.height = scr.height
        }
        requestActivate()
        Qt.callLater(function () { keyCatcher.forceActiveFocus() })
    }

    // After the phone locks / screen turns off and you wake it, Qt can resume
    // with the scene left blank (the Android surface is recreated). Re-assert
    // the full-screen geometry and force a repaint whenever we come back.
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive) {
                if (Qt.platform.os === "android" && !overlay.visible)
                    App.overlayOpen = true
                const scr = overlay.screen
                if (scr) {
                    overlay.x = scr.virtualX
                    overlay.y = scr.virtualY
                    overlay.width = scr.width
                    overlay.height = scr.height
                }
                overlay.requestUpdate()
                overlay.contentItem.update()
                requestActivate()
            }
        }
    }

    // Verbose check of the platform-reported safe area (status bar + camera
    // cutout at the top, navigation bar at the bottom) — surfaces in logcat so
    // we can confirm the insets track the system navigation mode setting.
    onSafeAreaMarginsChanged: console.warn("[SCRIPTURE] safe T/B/L/R " + overlay.SafeArea.margins.top + "/" + overlay.SafeArea.margins.bottom + "/" + overlay.SafeArea.margins.left + "/" + overlay.SafeArea.margins.right)
    Component.onCompleted: Qt.callLater(function () { console.warn("[SCRIPTURE] safe T/B/L/R " + overlay.SafeArea.margins.top + "/" + overlay.SafeArea.margins.bottom + "/" + overlay.SafeArea.margins.left + "/" + overlay.SafeArea.margins.right) })

    // ------------------------------------------------------------------ UI fragments

    component OverlayButton: Button {
        id: cell
        property color fg: "white"
        property int padX: 14
        property int padY: 4
        property string tip: ""
        property real radius: 6

        font.family: "Segoe UI"
        font.pixelSize: 11
        implicitWidth: contentItem.implicitWidth + padX * 2
        implicitHeight: contentItem.implicitHeight + padY * 2
        opacity: enabled ? 1 : 0.25

        background: Rectangle {
            radius: cell.radius
            color: "transparent"
            border.color: cell.enabled ? Qt.rgba(1, 1, 1, 0.35) : Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
        }

        contentItem: Text {
            text: cell.text
            color: cell.enabled ? cell.fg : Qt.rgba(1, 1, 1, 0.25)
            font: cell.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        ToolTip.visible: cell.hovered && cell.tip !== ""
        ToolTip.text: cell.tip
        ToolTip.delay: 500
        hoverEnabled: true
    }

    // Decorative Latin cross drawn from rectangles, not text.
    component CrossMark: Item {
        id: mark
        property color cr: Qt.rgba(1, 1, 1, 0.55)
        property real cellW: 25
        property real cellH: 42

        width: mark.beamW
        height: mark.stemH

        readonly property real beamW: 11 * cellW
        readonly property real stemW: 3 * cellW
        readonly property real beamH: 2 * cellH
        readonly property real stemH: 13 * cellH
        readonly property real beamTop: 3 * cellH

        Rectangle {
            anchors.horizontalCenter: mark.horizontalCenter
            width: mark.stemW
            height: mark.stemH
            color: mark.cr
            radius: mark.stemW / 3
        }
        Rectangle {
            anchors.horizontalCenter: mark.horizontalCenter
            width: mark.beamW
            height: mark.beamH
            y: mark.beamTop
            color: mark.cr
            radius: mark.beamH / 4
        }
    }

    // ------------------------------------------------------------------ Layers

    Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: {
            if (App.settingsOpen)
                App.settingsOpen = false
            else
                App.close_overlay()
        }
        Keys.onBackPressed: {
            if (App.settingsOpen)
                App.settingsOpen = false
            else
                App.close_overlay()
        }
        Keys.onReturnPressed: if (!App.loading && !App.settingsOpen) App.refresh()

        // -------- Overlay layer: the verse scrim on the dark background --------
        Item {
            id: overlayLayer
            anchors.fill: parent
            visible: App.overlayOpen && !App.settingsOpen

            // Deep scrim — same 78% black as the Omarchy speed-test overlay.
            Rectangle {
                id: scrim
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.78)
            }

            // Bare-scrim click dismisses; everything inside the cluster is swallowed.
            MouseArea {
                anchors.fill: parent
                onClicked: App.close_overlay()
            }

            // Decorative Latin cross, centered behind the verse text.
            CrossMark {
                anchors.centerIn: parent
                cellW: 24
                cellH: 48
                cr: Qt.rgba(1, 1, 1, 0.12)
                opacity: App.loading ? 0.45 : 1
                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
            }

            // Content cluster: fills the screen width; scrolls vertically when a
            // passage is taller than the screen (no more shrink-to-fit scaling).
            Flickable {
                id: clusterFlick
                anchors.fill: parent
                anchors.topMargin: parent.SafeArea.margins.top + 4
                anchors.bottomMargin: parent.SafeArea.margins.bottom + 4
                anchors.leftMargin: parent.SafeArea.margins.left + 16
                anchors.rightMargin: parent.SafeArea.margins.right + 16
                clip: true
                contentWidth: width
                contentHeight: Math.max(holder.height, height)
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 4
                    rightPadding: 2
                }

                Item {
                    id: holder
                    width: clusterFlick.width
                    height: Math.max(cluster.implicitHeight, clusterFlick.height)

                    ColumnLayout {
                        id: cluster
                        width: holder.width
                        anchors.horizontalCenter: holder.horizontalCenter
                        y: Math.max(0, (holder.height - cluster.height) / 2)
                        spacing: 18

                    // A — translation name
                    Text {
                        Layout.fillWidth: true
                        visible: App.translationLabel !== ""
                        text: App.translationLabel
                        color: Qt.rgba(1, 1, 1, 0.55)
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 2
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: App.translationAttribution !== ""
                        textFormat: Text.PlainText
                        text: App.translationAttribution
                        color: Qt.rgba(1, 1, 1, 0.45)
                        font.family: "Segoe UI"
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    // B — verse text with typewriter reveal (rich text)
                    Text {
                        Layout.fillWidth: true
                        text: App.displayText
                        textFormat: Text.RichText
                        color: "white"
                        font.family: "Segoe UI"
                        font.pixelSize: 28
                        font.weight: Font.Light
                        lineHeight: 1.55
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        opacity: App.loading ? 0.45 : 1
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                    }

                    // C — verse reference (the only accent on the overlay)
                    Text {
                        Layout.fillWidth: true
                        visible: App.verseReference !== ""
                        text: App.verseReference
                        color: "#faa968"
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.5
                        horizontalAlignment: Text.AlignHCenter
                    }

                    // D — toolbar
                    RowLayout {
                        id: toolbar
                        Layout.fillWidth: true
                        spacing: 6

                        OverlayButton {
                        text: "◀"
                            tip: "Previous verse in this session"
                            padX: 10
                            enabled: App.histCanBack
                            onClicked: App.back()
                        }
                        OverlayButton {
                        text: "▶"
                            tip: "Next verse in this session"
                            padX: 10
                            enabled: App.histCanForward
                            onClicked: App.forward()
                        }
                        OverlayButton {
                        text: App.fixedReference !== "" ? "Repeat" : "Another Verse"
                            tip: App.fixedReference !== "" ? "Show the fixed verse again" : "Get a different random verse"
                            padX: 14
                            fg: "white"
                            enabled: !App.loading
                            onClicked: App.refresh()
                            opacity: App.loading ? 0 : 1
                            Behavior on opacity { NumberAnimation { duration: 240 } }
                        }
                        OverlayButton {
                        text: App.starSymbol
                            tip: App.isFavorite ? "Remove from favorites" : "Save to favorites"
                            padX: 10
                            fg: App.isFavorite ? "#f5c542" : Qt.rgba(1, 1, 1, 0.55)
                            enabled: App.anchor !== "" && !App.loading
                            onClicked: App.toggle_favorite()
                        }
                        OverlayButton {
                        text: App.translationId === "esv" ? "Open on esv.org" : "Open in browser"
                            tip: "Read the passage online"
                            padX: 10
                            fg: Qt.rgba(1, 1, 1, 0.55)
                            enabled: App.verseReference !== ""
                            onClicked: App.open_in_browser(App.verseReference)
                        }
                    }

                    // E — update chip (stub singleton on Android: never visible)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: Updater.updateAvailable || Updater.downloading || Updater.updateApplied
                                    || Updater.updateError !== ""

                        ColumnLayout {
                            spacing: 3
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: Updater.downloading ? "Downloading update " + Math.round(Updater.updateProgress * 100) + "%" :
                                      Updater.updateApplied ? "Applying update — Scripture will restart\u2026" :
                                      Updater.updateError !== "" ? "Update failed" :
                                      "Update available: " + Updater.updateTag
                                color: Updater.updateError !== "" ? "#ff6b6b" : "#f5c542"
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                visible: Updater.updateError !== ""
                                text: Updater.updateError
                                color: Qt.rgba(1, 1, 1, 0.6)
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                            }
                        }
                        OverlayButton {
                            text: Updater.downloading ? "Downloading\u2026" :
                                  Updater.updateApplied ? "Restarting\u2026" :
                                  Updater.updateError !== "" ? "Retry" : "Download"
                            tip: Updater.updateError !== "" ? "Retry the self-update" : "Download and install the update automatically"
                            padX: 10
                            fg: "white"
                            enabled: !Updater.downloading && !Updater.updateApplied
                            onClicked: Updater.download()
                        }
                        OverlayButton {
                            text: "Open browser"
                            tip: "Open the release page in your browser"
                            padX: 10
                            fg: Qt.rgba(1, 1, 1, 0.55)
                            enabled: !Updater.downloading
                            onClicked: Updater.open()
                        }
                    }

                    // F — favorites chips (at most 8 + overflow note)
                    RowLayout {
                        Layout.fillWidth: true
                        visible: App.favoritesOverflow + (App.favoritesChips.length > 0 ? 1 : 0) > 0
                        spacing: 6

                        Repeater {
                            model: App.favoritesChips
                            OverlayButton {
                                text: modelData
                                tip: "Open " + modelData
                                padX: 8
                                padY: 2
                                fg: App.anchor === modelData ? "white" : Qt.rgba(1, 1, 1, 0.55)
                                onClicked: App.load_reference(modelData)
                            }
                        }

                        Text {
                            visible: App.favoritesOverflow > 0
                            text: "+" + App.favoritesOverflow + " more"
                            color: Qt.rgba(1, 1, 1, 0.55)
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                        }
                    }

                    // F — jump to any reference
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "JUMP TO"
                            color: Qt.rgba(1, 1, 1, 0.55)
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 2
                            verticalAlignment: Text.AlignVCenter
                        }

                        Rectangle {
                            Layout.preferredWidth: 240
                            Layout.preferredHeight: 34
                            radius: 6
                            color: Qt.rgba(1, 1, 1, 0.12)
                            border.color: Qt.rgba(1, 1, 1, 0.35)

                            TextInput {
                                id: jumpField
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: Text.AlignVCenter
                                color: "white"
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                                selectByMouse: true
                                maximumLength: 120
                                onAccepted: {
                                    App.load_reference(text)
                                    text = ""
                                }
                            }

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                verticalAlignment: Text.AlignVCenter
                                visible: jumpField.text === ""
                                text: "e.g. John 3:16"
                                color: Qt.rgba(1, 1, 1, 0.25)
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                            }
                        }

                        OverlayButton {
                            text: "Go"
                            tip: "Jump to that reference"
                            padX: 12
                            fg: Qt.rgba(1, 1, 1, 0.55)
                            onClicked: {
                                App.load_reference(jumpField.text)
                                jumpField.text = ""
                            }
                        }
                    }

                    // G — fetch notice / H — error
                    Text {
                        Layout.fillWidth: true
                        visible: App.fetchNotice !== ""
                        text: App.fetchNotice
                        color: Qt.rgba(1, 1, 1, 0.55)
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: App.errorText !== ""
                        text: App.errorText
                        color: "#ff6b6b"
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                    }
                }
            }

            // Explicit, always-visible close affordance. The overlay can also be
            // dismissed with Esc/Back or a bare-scrim tap.
            OverlayButton {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: parent.SafeArea.margins.top + 8
                anchors.rightMargin: parent.SafeArea.margins.right + 8
                text: "\u2715  Close"
                tip: "Close the overlay (Esc/Back)"
                padX: 16
                padY: 7
                radius: 17
                fg: "white"
                onClicked: App.close_overlay()
            }
        }

        // -------- Settings layer: full-screen embedded settings panel --------

        Item {
            id: settingsLayer
            anchors.fill: parent
            visible: App.settingsOpen
             onVisibleChanged: if (visible) Qt.callLater(function() { settingsPanel.seedSettings() })


            ScriptureSettings {
                id: settingsPanel
                anchors.fill: parent
            }
        }
    }
}