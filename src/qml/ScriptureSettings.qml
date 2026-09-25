import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import ScriptureRT 1.0

// Settings panel, embedded in the app window (the desktop app hosted it as a
// separate QQuickView window; Android has a single window, so it lives here as
// a full-screen layer instead). It mirrors `App.*` settings state into the
// form; main.qml calls seedSettings() each time it becomes visible.

Item {
    id: settingsRoot
    implicitWidth: 440
    implicitHeight: 560
    focus: visible
    Keys.onEscapePressed: App.settingsOpen = false
    Keys.onBackPressed: App.settingsOpen = false

    Rectangle {
        anchors.fill: parent
        color: "#111418"
    }

    Flickable {
        id: settingsFlick
        anchors.fill: parent
        anchors.leftMargin: parent.SafeArea.margins.left + 18
        anchors.rightMargin: parent.SafeArea.margins.right + 18
        anchors.topMargin: parent.SafeArea.margins.top + 12
        anchors.bottomMargin: parent.SafeArea.margins.bottom + 12
        contentWidth: width
        contentHeight: settingsColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: settingsColumn
            width: settingsFlick.width
            spacing: 12

        Text {
            text: "ESV API KEY"
            color: "#faa968"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 2
        }

        Text {
            text: "A free api.esv.org key enables the English Standard Version. " +
                  "The key is stored in the device keystore and is excluded from backup. " +
                  "Without one the ESV falls back to the World English Bible."
            color: "#9aa0a6"
            font.family: "Segoe UI"
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        TextField {
            id: keyField
            Layout.fillWidth: true
            placeholderText: "Paste your ESV API key (optional)"
            echoMode: TextInput.Password
            maximumLength: 512
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Layout.preferredHeight: 1
            color: "#2a2f35"
        }

        Text {
            text: "VERSE & SCHEDULE"
            color: "#faa968"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 2
        }

        RowLayout {
            spacing: 6

            Text {
                text: "Translation"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.fillWidth: true
            }

            Button { text: "ESV"; checkable: true; checked: settingsRoot.selTranslation === "ESV"; onClicked: settingsRoot.selTranslation = "ESV" }
            Button { text: "WEB"; checkable: true; checked: settingsRoot.selTranslation === "WEB"; onClicked: settingsRoot.selTranslation = "WEB" }
            Button { text: "KJV"; checkable: true; checked: settingsRoot.selTranslation === "KJV"; onClicked: settingsRoot.selTranslation = "KJV" }
        }

        RowLayout {
            spacing: 8

            Text {
                text: "Fixed verse"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.fillWidth: true
            }

            TextField {
                id: fixedField
                Layout.preferredWidth: 150
                placeholderText: "e.g. John 3:16"
                maximumLength: 120
            }
        }

        RowLayout {
            spacing: 8

            Text {
                text: "Auto-open (HH:MM)"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.fillWidth: true
            }

            TextField {
                id: autoField
                Layout.preferredWidth: 120
                placeholderText: "07:30"
                maximumLength: 5
            }
        }

        RowLayout {
            spacing: 8

            Text {
                text: "Daily verse (HH:MM)"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.fillWidth: true
            }

            TextField {
                id: dailyField
                Layout.preferredWidth: 120
                placeholderText: "08:00"
                maximumLength: 5
            }
        }

        RowLayout {
            visible: App.notificationAvailable && (dailyField.text !== "" || App.settingsDailyNotificationAt !== "")
            spacing: 8

            Text {
                text: App.notificationPermissionState === "granted" ? "Notifications allowed" :
                      App.notificationPermissionState === "pending" ? "Permission prompt pending" :
                      App.notificationPermissionState === "app-blocked" ? "Notifications blocked in Android Settings" :
                      App.notificationPermissionState === "channel-disabled" ? "Daily verse channel disabled" :
                      App.notificationPermissionRequested ? "Notification permission denied" :
                      "Notification permission required"
                color: App.notificationPermissionGranted ? "#9aa0a6" : "#faa968"
                font.family: "Segoe UI"
                font.pixelSize: 10
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            Button {
                objectName: "notificationPermissionButton"
                text: App.notificationPermissionGranted ? "Enabled" :
                      App.notificationPermissionState === "pending" ? "Pending" :
                      (App.notificationPermissionState === "app-blocked"
                       || App.notificationPermissionState === "channel-disabled"
                       || (App.notificationPermissionState === "runtime-denied"
                           && App.notificationPermissionRequested)) ? "Open Settings" :
                      App.notificationPermissionRequested ? "Denied" : "Enable"
                enabled: App.notificationPermissionState === "app-blocked"
                        || App.notificationPermissionState === "channel-disabled"
                        || App.notificationPermissionState === "runtime-denied"
                onClicked: {
                    if (App.notificationPermissionState === "app-blocked"
                            || App.notificationPermissionState === "channel-disabled"
                            || (App.notificationPermissionState === "runtime-denied"
                                && App.notificationPermissionRequested))
                        App.open_notification_settings()
                    else if (App.notificationPermissionState === "runtime-denied"
                             && !App.notificationPermissionRequested)
                        App.request_notification_permission()
                }
            }
        }

        Text {
            text: "DISPLAY"
            color: "#faa968"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 2
        }

        RowLayout {
            spacing: 8

            Text {
                text: "Verse font size"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.preferredWidth: 120
            }

            Slider {
                id: fontSlider
                Layout.fillWidth: true
                from: 16
                to: 56
                stepSize: 1
                value: App.verseFontSize
                onMoved: App.verseFontSize = value
            }

            Text {
                text: Math.round(fontSlider.value)
                color: "#e8eaed"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.preferredWidth: 28
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            spacing: 8

            Text {
                text: "Scrim opacity"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.preferredWidth: 120
            }

            Slider {
                id: scrimSlider
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: App.scrimOpacity
                onMoved: App.scrimOpacity = value
            }

            Text {
                text: Math.round(scrimSlider.value) + "%"
                color: "#e8eaed"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.preferredWidth: 38
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            spacing: 8

            Text {
                text: "Reveal speed"
                color: "#9aa0a6"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.preferredWidth: 120
            }

            Slider {
                id: revealSlider
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: App.revealSpeed
                onMoved: App.revealSpeed = value
            }

            Text {
                text: revealSlider.value === 0 ? "Off" : Math.round(revealSlider.value) + "%"
                color: "#e8eaed"
                font.family: "Segoe UI"
                font.pixelSize: 11
                Layout.preferredWidth: 38
                horizontalAlignment: Text.AlignRight
            }
        }

        Text {
            text: "Reveal speed 0 shows the complete verse immediately. Scrim opacity 0 removes the dark layer."
            color: "#9aa0a6"
            font.family: "Segoe UI"
            font.pixelSize: 10
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Button {
            text: "Apply"
            Layout.fillWidth: true
            onClicked: App.save_all_settings(keyField.text, settingsRoot.selTranslation, fixedField.text, autoField.text, dailyField.text, Math.round(fontSlider.value), Math.round(scrimSlider.value), Math.round(revealSlider.value))
        }

        Text {
            visible: App.settingsNotice !== ""
            text: App.settingsNotice
            color: App.settingsNoticeError ? "#ff6b6b" : "#9aa0a6"
            font.family: "Segoe UI"
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Layout.preferredHeight: 1
            color: "#2a2f35"
        }

        Text {
            text: "FAVORITES"
            color: "#faa968"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 2
        }

        Text {
            visible: App.favorites.length === 0
            text: "No favorites yet — tap ☆ on any verse to save it."
            color: "#9aa0a6"
            font.family: "Segoe UI"
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(App.favorites.length * 34, 170)
            visible: App.favorites.length > 0
            color: "transparent"
            clip: true

            Flickable {
                anchors.fill: parent
                contentHeight: favCol.implicitHeight
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: favCol
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: App.favorites

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                color: "#e8eaed"
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Button {
                                text: "▶"
                                ToolTip.visible: hovered
                                ToolTip.text: "Load " + modelData
                                onClicked: {
                                    App.settingsOpen = false
                                    App.load_reference(modelData)
                                }
                            }

                            Button {
                                text: "×"
                                ToolTip.visible: hovered
                                ToolTip.text: "Remove " + modelData + " from favorites"
                                onClicked: App.remove_favorite(modelData)
                            }
                        }
                    }
                }
            }
        }

        Button {
            text: "Close"
            Layout.fillWidth: true
            onClicked: App.settingsOpen = false
        }
        }
    }

    // Refresh the form from the app settings each time the panel opens.
    function seedSettings() {
        keyField.text = App.settingsApiKey
        fixedField.text = App.settingsFixedReference
        autoField.text = App.settingsAutoOpenAt
        dailyField.text = App.settingsDailyNotificationAt
        fontSlider.value = App.verseFontSize
        scrimSlider.value = App.scrimOpacity
        revealSlider.value = App.revealSpeed
        selTranslation = App.settingsTranslation
    }
    property string selTranslation: "ESV"
}