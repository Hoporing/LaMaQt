import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15
import QtQuick.Layouts 1.15
import CompactLaMaQt 1.0

// ============================================================================
//  Main.qml  -  Application root window
// ============================================================================
ApplicationWindow {
    id: root
    title:         "LaMaQt"
    width:         1440
    height:        860
    minimumWidth:  1100
    minimumHeight: 700
    visible:       true
    color:         Theme.bgDark

    // ── Placement ────────────────────────────────────────────────────────────
    Component.onCompleted: {
        x = (Screen.width  - width)  / 2
        y = (Screen.height - height) / 2
    }

    // ── Ctrl+S shortcut ───────────────────────────────────────────────────────
    Shortcut {
        sequence:    "Ctrl+S"
        enabled:     workspace.resultVersion > 0
        onActivated: workspace.openSaveDialog()
    }

    // ── Menu bar ──────────────────────────────────────────────────────────────
    menuBar: MenuBar {
        implicitHeight: Theme.menuBarHeight
        background: Rectangle {
            color: Theme.bgPanel
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1
                color: Theme.divider
            }
        }

        delegate: MenuBarItem {
            implicitHeight: Theme.menuBarHeight
            leftPadding:  10
            rightPadding: 10

            contentItem: Text {
                text:                parent.text
                font.family:         Theme.fontFamily
                font.pixelSize:      Theme.fontSizeNormal
                color:               parent.highlighted ? Theme.textPrimary
                                                        : Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:   Text.AlignVCenter
            }

            background: Rectangle {
                color: parent.highlighted ? Theme.bgLight : "transparent"
            }
        }

        // ── File ──────────────────────────────────────────────────────────────
        Menu {
            title: "File"
            topPadding: 0; bottomPadding: 0
            palette.window:          Theme.bgPanel
            palette.text:            Theme.textPrimary
            palette.highlightedText: Theme.textPrimary
            palette.highlight:       Theme.bgLighter
            palette.mid:             Theme.textDisabled

            MenuItem {
                text:           "Save Inpainted Result (Ctrl+S)"
                enabled:        workspace.resultVersion > 0
                implicitHeight: Theme.menuBarHeight
                onTriggered:    workspace.openSaveDialog()
            }
            MenuSeparator {}
            MenuItem {
                text:           "Exit (&X)"
                implicitHeight: Theme.menuBarHeight
                onTriggered:    Qt.quit()
            }
        }

        // ── Help ──────────────────────────────────────────────────────────────
        Menu {
            title: "Help"
            topPadding: 0; bottomPadding: 0
            implicitHeight: Theme.menuItemHeight
            palette.window:          Theme.bgPanel
            palette.text:            Theme.textPrimary
            palette.highlightedText: Theme.textPrimary
            palette.highlight:       Theme.bgLighter

            MenuItem {
                text:           "About LaMaQt (&A)"
                implicitHeight: Theme.menuBarHeight
                onTriggered:    aboutDialog.open()
            }
        }
    }

    // ── Main content: SplitView ───────────────────────────────────────────────
    SplitView {
        id:           mainSplit
        anchors.fill: parent
        orientation:  Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 5
            color: SplitHandle.pressed ? Theme.accent :
                                         SplitHandle.hovered ? Theme.borderFocus : Theme.divider
            Behavior on color { ColorAnimation { duration: 100 } }
        }

        // ── Left: Source panel ─────────────────────────────────────────────
        SourcePanel {
            id: sourcePanel
            SplitView.preferredWidth: 285
            SplitView.minimumWidth:   220
            SplitView.maximumWidth:   400

            onFrameCaptured: function(base64) {
                workspace.setSourceImage(base64)
            }
        }

        // ── Right: Workspace panel ─────────────────────────────────────────
        WorkspacePanel {
            id: workspace
            SplitView.fillWidth:    true
            SplitView.minimumWidth: 700
        }
    }

    // ── About dialog ──────────────────────────────────────────────────────────
    AboutDialog {
        id: aboutDialog
    }
}
