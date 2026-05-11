import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import CompactLaMaQt 1.0

// ============================================================================
//  SourcePanel  -  Left panel: video/RTSP playback + frame capture
//
//  Emits:
//    frameCaptured(base64)  - when user clicks Capture or Load Image
// ============================================================================
Item {
    id: root
    signal frameCaptured(string base64)

    // File URL of the currently loaded static image (set by imageFileDialog).
    property url loadedImageUrl: ""

    // Images larger than this (on either axis) are downscaled before processing.
    readonly property int maxImageDim: 3840

    // ── Root layout ────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: Theme.marginNormal
        spacing: Theme.marginNormal

        // ── Video display ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            color:   "#000000"
            radius:  Theme.radiusNormal
            clip:    true

            VideoSurface {
                id: videoItem
                anchors.fill: parent
            }

            // Status overlay
            Rectangle {
                anchors.bottom:  parent.bottom
                anchors.left:    parent.left
                anchors.margins: 6
                width:  statusRow.implicitWidth + 12
                height: 22
                radius: 4
                color:  "#99000000"
                visible: videoItem.statusText.length > 0

                Row {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 5

                    Rectangle {
                        width: 7; height: 7
                        radius: 4
                        color: videoItem.playing ? Theme.statusReady : Theme.statusOff
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text:           videoItem.statusText
                        color:          Theme.textSecondary
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // FPS badge
            Rectangle {
                anchors.top:    parent.top
                anchors.right:  parent.right
                anchors.margins: 6
                width:  fpsText.implicitWidth + 10
                height: 20
                radius: 3
                color:  "#99000000"
                visible: videoItem.playing && videoItem.fps > 0

                Text {
                    id: fpsText
                    anchors.centerIn: parent
                    text:           videoItem.fps.toFixed(1) + " fps"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            // Static image preview (replaces video view when an image is loaded)
            Image {
                anchors.fill: parent
                fillMode:     Image.PreserveAspectFit
                smooth:       true
                cache:        false
                source:       (imageLoader.status === Image.Ready && !videoItem.playing)
                              ? imageLoader.source : ""
            }

            // Image resolution badge (top-left, shown when image is loaded)
            Rectangle {
                anchors.top:     parent.top
                anchors.left:    parent.left
                anchors.margins: 6
                width:  imgInfoText.implicitWidth + 12
                height: 20; radius: 3
                color:  "#99000000"
                visible: imageLoader.status === Image.Ready && !videoItem.playing

                Text {
                    id: imgInfoText
                    anchors.centerIn: parent
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    text: {
                        var w = imageLoader.implicitWidth
                        var h = imageLoader.implicitHeight
                        var sc = Math.min(1.0, Math.min(root.maxImageDim / w, root.maxImageDim / h))
                        if (sc < 0.999) {
                            return w + "\u00D7" + h
                                    + "  \u2192  "
                                    + Math.round(w * sc) + "\u00D7" + Math.round(h * sc)
                        }
                        return w + "\u00D7" + h
                    }
                }
            }

            // Placeholder text when no video and no image loaded
            Text {
                anchors.centerIn: parent
                visible: !videoItem.playing && imageLoader.status !== Image.Ready
                text:    "No video source\nOpen a file or connect RTSP"
                color:   Theme.textDisabled
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // ── Playback controls ───────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.marginSmall

            StyledButton {
                text:              "Open File"
                Layout.fillWidth:  true
                onClicked:         fileDialog.open()
            }
            StyledButton {
                text:              "Stop"
                normalColor:       Theme.btnDanger
                hoverColor:        Qt.lighter(Theme.btnDanger, 1.2)
                pressColor:        Qt.darker(Theme.btnDanger, 1.1)
                enabled:           videoItem.playing
                Layout.fillWidth:  true
                onClicked:         videoItem.stop()
            }
        }

        // ── RTSP section ────────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text:           "RTSP Stream"
                color:          Theme.textSecondary
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                font.weight:    Font.Medium
            }

            TextField {
                id: rtspUrl
                Layout.fillWidth: true
                placeholderText:  "rtsp://host:port/stream"
                color:            Theme.textPrimary
                font.family:      Theme.fontFamily
                font.pixelSize:   Theme.fontSizeSmall
                leftPadding:      8
                rightPadding:     8

                background: Rectangle {
                    color:        Theme.bgInput
                    radius:       Theme.radiusSmall
                    border.color: rtspUrl.activeFocus ? Theme.borderFocus : Theme.border
                    border.width: 1
                }

                Keys.onReturnPressed: connectRtsp()
            }

            // Collapsible auth fields
            ColumnLayout {
                id: authSection
                Layout.fillWidth: true
                spacing: 4
                visible: authToggle.checked

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "ID"
                        color: Theme.textDisabled
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        Layout.preferredWidth: 22
                    }
                    TextField {
                        id: rtspId
                        Layout.fillWidth: true
                        placeholderText:  "username"
                        color:            Theme.textPrimary
                        font.family:      Theme.fontFamily
                        font.pixelSize:   Theme.fontSizeSmall
                        leftPadding:      6
                        background: Rectangle {
                            color: Theme.bgInput; radius: Theme.radiusSmall
                            border.color: rtspId.activeFocus ? Theme.borderFocus : Theme.border
                            border.width: 1
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "PW"
                        color: Theme.textDisabled
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        Layout.preferredWidth: 22
                    }
                    TextField {
                        id: rtspPw
                        Layout.fillWidth: true
                        placeholderText:  "password"
                        echoMode:         TextField.Password
                        color:            Theme.textPrimary
                        font.family:      Theme.fontFamily
                        font.pixelSize:   Theme.fontSizeSmall
                        leftPadding:      6
                        background: Rectangle {
                            color: Theme.bgInput; radius: Theme.radiusSmall
                            border.color: rtspPw.activeFocus ? Theme.borderFocus : Theme.border
                            border.width: 1
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.marginSmall

                // Auth toggle button
                Rectangle {
                    width: 80; height: 32
                    radius: Theme.radiusSmall
                    color:  authToggle.checked ? Qt.rgba(0.2, 0.5, 0.8, 0.3) : Theme.bgLight
                    border.color: Theme.border; border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text:  authToggle.checked ? "▲ Auth" : "▼ Auth"
                        color: Theme.textSecondary
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                    CheckBox {
                        id: authToggle
                        anchors.fill: parent
                        checked: false
                        contentItem: Item {}
                        indicator:   Item {}
                        background:  Item {}
                    }
                }

                StyledButton {
                    text:             "Connect"
                    Layout.fillWidth: true
                    onClicked:        connectRtsp()
                }
            }
        }

        // ── Divider ─────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth:    true
            height:              1
            color:               Theme.divider
        }

        // ── Capture / Load buttons ───────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.marginSmall

            // ── Video: Capture Frame ──────────────────────────────────────────
            StyledButton {
                text:             "Capture Frame"
                enabled:          videoItem.playing
                Layout.fillWidth: true
                implicitHeight:   36
                normalColor:      Theme.btnSuccess
                hoverColor:       Qt.lighter(Theme.btnSuccess, 1.2)
                pressColor:       Qt.darker(Theme.btnSuccess, 1.1)

                onClicked: {
                    var b64 = videoItem.grabFrameBase64()
                    if (b64.length > 0) root.frameCaptured(b64)
                }
            }

            // ── Image: Open + Load as Source ─────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.marginSmall

                StyledButton {
                    text:            "Open Image File"
                    Layout.fillWidth: true
                    implicitHeight:   34
                    normalColor:      Theme.bgLighter
                    hoverColor:       Theme.bgLight
                    pressColor:       Theme.bgMedium
                    onClicked:        imageFileDialog.open()
                }

                StyledButton {
                    text:             "Load as Source"
                    Layout.fillWidth: true
                    implicitHeight:   34
                    enabled:          imageLoader.status === Image.Ready
                    normalColor:      Theme.btnSuccess
                    hoverColor:       Qt.lighter(Theme.btnSuccess, 1.2)
                    pressColor:       Qt.darker(Theme.btnSuccess, 1.1)

                    onClicked: {
                        var b64 = imageUtils.loadFile(root.loadedImageUrl.toString(),
                                                      root.maxImageDim)
                        if (b64.length > 0) root.frameCaptured(b64)
                    }
                }
            }
        }
    }

    // ── File dialogs ───────────────────────────────────────────────────────────
    FileDialog {
        id: fileDialog
        title:       "Open Video File"
        nameFilters: ["Video files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.ts *.webm)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            videoItem.open(path, 1280, 720)
        }
    }

    FileDialog {
        id: imageFileDialog
        title:       "Open Image File"
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.bmp *.tiff *.webp)", "All files (*)"]
        onAccepted: {
            root.loadedImageUrl = selectedFile
            imageLoader.source  = selectedFile
        }
    }

    // Image loader for static image files (used for preview only).
    // Actual file → base64 conversion is handled by imageUtils.loadFile() in C++.
    Image {
        id: imageLoader
        visible: false
        cache:   false
    }

    // ── RTSP connect helper ────────────────────────────────────────────────────
    function connectRtsp() {
        var url = rtspUrl.text.trim()
        if (url.length === 0) return
        if (authToggle.checked && rtspId.text.trim().length > 0 &&
                !url.includes("@"))
        {
            // Inject credentials: rtsp://user:pass@host/path
            url = url.replace("rtsp://", "rtsp://" +
                              encodeURIComponent(rtspId.text.trim()) + ":" +
                              encodeURIComponent(rtspPw.text.trim()) + "@")
        }
        videoItem.open(url, 1280, 720)
    }
}
