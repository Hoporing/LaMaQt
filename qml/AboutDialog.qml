import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: root
    title:   "About LaMaQt"
    width:   500
    height:  520
    modal:   true
    anchors.centerIn: parent

    background: Rectangle {
        color:        Theme.bgPanel
        radius:       Theme.radiusNormal
        border.color: Theme.border
        border.width: 1
    }

    header: Rectangle {
        width:  parent.width
        height: 44
        color:  Theme.bgLight
        radius: Theme.radiusNormal

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: Theme.radiusNormal
            color: parent.color
        }

        Text {
            anchors.centerIn: parent
            text:           "About LaMaQt"
            color:          Theme.textPrimary
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
            font.weight:    Font.Medium
        }
    }

    // ── Scrollable content ────────────────────────────────────────────────────
    ScrollView {
        anchors.fill:        parent
        contentWidth:        availableWidth
        clip:                true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        Column {
            width:   parent.width
            padding: Theme.marginLarge
            spacing: 0

            // ── App name & description ──────────────────────────────────────
            Text {
                text:           "LaMaQt  v1.0.0"
                color:          Theme.textPrimary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeLarge
                font.weight:    Font.Bold
                bottomPadding:  6
            }

            Text {
                text:  "Image inpainting and segmentation GUI\npowered by LaMa and SAM 2 via ONNX Runtime."
                color: Theme.textSecondary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                wrapMode:       Text.WordWrap
                width:          parent.width - parent.padding * 2
                bottomPadding:  Theme.marginLarge
            }

            // ── Divider ─────────────────────────────────────────────────────
            Rectangle { width: parent.width - parent.padding * 2; height: 1; color: Theme.divider }

            // ── Model Licenses ───────────────────────────────────────────────
            Text {
                text:           "AI Models"
                color:          Theme.textPrimary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                font.weight:    Font.Medium
                topPadding:     Theme.marginNormal
                bottomPadding:  6
            }

            Text {
                text: "LaMa — Large Mask Inpainting, Fourier Convolutions\n" +
                      "  Authors: Roman Suvorov et al., Samsung Research\n" +
                      "  License: Apache 2.0\n" +
                      "  https://github.com/advimman/lama\n\n" +
                      "SAM 2 — Segment Anything Model 2\n" +
                      "  Authors: Meta AI Research\n" +
                      "  License: Apache 2.0\n" +
                      "  https://github.com/facebookresearch/segment-anything-2"
                color:          Theme.textSecondary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                lineHeight:     1.5
                wrapMode:       Text.WordWrap
                width:          parent.width - parent.padding * 2
                bottomPadding:  Theme.marginLarge
            }

            // ── Divider ─────────────────────────────────────────────────────
            Rectangle { width: parent.width - parent.padding * 2; height: 1; color: Theme.divider }

            // ── Runtime Libraries ────────────────────────────────────────────
            Text {
                text:           "Runtime Libraries"
                color:          Theme.textPrimary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                font.weight:    Font.Medium
                topPadding:     Theme.marginNormal
                bottomPadding:  6
            }

            Text {
                text: "Qt 6.10            —  LGPL v3\n" +
                      "ONNX Runtime 1.24  —  MIT\n" +
                      "OpenCV 4.13        —  Apache 2.0\n" +
                      "libVLC             —  LGPL v2.1\n" +
                      "CUDA / cuDNN       —  NVIDIA Proprietary"
                color:          Theme.textSecondary
                font.family:    "Consolas"
                font.pixelSize: Theme.fontSizeSmall
                lineHeight:     1.6
                bottomPadding:  Theme.marginLarge
            }

            // ── Divider ─────────────────────────────────────────────────────
            Rectangle { width: parent.width - parent.padding * 2; height: 1; color: Theme.divider }

            // ── Qt LGPL notice ───────────────────────────────────────────────
            Text {
                text:           "Qt LGPL Notice"
                color:          Theme.textPrimary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                font.weight:    Font.Medium
                topPadding:     Theme.marginNormal
                bottomPadding:  6
            }

            Text {
                text:  "This application uses the Qt library under the GNU Lesser\n" +
                       "General Public License v3 (LGPL v3). Qt is dynamically\n" +
                       "linked. Source code is available at https://qt.io"
                color: Theme.textSecondary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                lineHeight:     1.5
                wrapMode:       Text.WordWrap
                width:          parent.width - parent.padding * 2
                bottomPadding:  Theme.marginNormal
            }
        }
    }

    footer: Rectangle {
        width:  parent.width
        height: 44
        color:  Theme.bgMedium
        radius: Theme.radiusNormal

        Rectangle {
            anchors.top: parent.top
            width: parent.width; height: Theme.radiusNormal
            color: parent.color
        }

        StyledButton {
            anchors.centerIn: parent
            text:         "Close"
            implicitWidth: 90
            onClicked:    root.close()
        }
    }
}
