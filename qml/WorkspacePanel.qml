import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import Qt.labs.settings 1.0
import CompactLaMaQt 1.0

// ============================================================================
//  WorkspacePanel  -  Right panel: mask editor + result viewer + controls
//
//  Modes:
//    0 = Brush   — manual brush painting (MaskPainter)
//    1 = SAM     — click-based object selection (SegmentCanvas + SAMBridge)
//
//  Workflow (Brush):
//    1. setSourceImage(base64) from Main.qml
//    2. User paints mask on MaskPainter
//    3. Run Inpainting → lamaBridge.inpaint()
//
//  Workflow (SAM):
//    1. setSourceImage(base64) — sets SegmentCanvas + (if SAM ready) starts encoding
//    2. Load SAM model → initializeDone → re-encode if image already loaded
//    3. Left-click (positive) / Right-click (negative) → samBridge.addPoint() → maskReady
//    4. Run Inpainting → lamaBridge.inpaint(segmentCanvas.getImageBase64(), samBridge.getMaskBase64())
// ============================================================================

Item {
    id: root

    // ── State ─────────────────────────────────────────────────────────────────
    property int    resultVersion:      0
    property int    editorMode:         0    // 0 = Brush, 1 = SAM
    property int    samInputMode:       0    // 0 = Point, 1 = Box  (SAM sub-mode)
    property string currentImageBase64: ""   // for re-encoding when SAM model loads later
    property string samStatusOverride:  ""   // error / status override for SAM bar

    // SAM variant lookup table
    readonly property var samVariants: [
        { label: "Tiny",   folder: "tiny",      fileId: "tiny"      },
        { label: "Small",  folder: "small",      fileId: "small"     },
        { label: "Base+",  folder: "base_plus",  fileId: "base_plus" },
        { label: "Large",  folder: "large",      fileId: "large"     }
    ]

    // ── Public API (called from Main.qml) ────────────────────────────────────
    function openSaveDialog() { saveDialog.open() }

    function setSourceImage(base64) {
        currentImageBase64 = base64
        maskPainter.setBaseImage(base64)
        segmentCanvas.setBaseImage(base64)
        // Only encode if SAM model is already loaded
        if (samBridge.ready)
            samBridge.setImage(base64)

        resultVersion    = 0
        resultImage.source = ""
        statusText.text  = "Mask ready — paint the area to inpaint"
        statusText.color = Theme.textSecondary
    }

    // ── Main layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing:      0

        // ── Canvas area (editor + result) ─────────────────────────────────────
        SplitView {
            id:                canvasSplit
            Layout.fillWidth:  true
            Layout.fillHeight: true
            orientation:       Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 5
                color: SplitHandle.pressed ? Theme.accent :
                                             SplitHandle.hovered ? Theme.borderFocus : Theme.divider
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            // ── Left: Editor panel ────────────────────────────────────────────
            Item {
                SplitView.preferredWidth: root.width * 0.5
                SplitView.minimumWidth:   300

                ColumnLayout {
                    anchors.fill: parent
                    spacing:      0

                    // ── Header with mode tabs ──────────────────────────────────
                    Rectangle {
                        Layout.fillWidth: true
                        height:           36
                        color:            Theme.bgPanel

                        RowLayout {
                            anchors.fill:        parent
                            anchors.leftMargin:  Theme.marginNormal
                            anchors.rightMargin: Theme.marginNormal
                            spacing:             6

                            // Status dot (reflects active canvas)
                            Rectangle {
                                width:  8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: editorMode === 0
                                       ? (maskPainter.hasImage    ? Theme.statusReady : Theme.statusOff)
                                       : (segmentCanvas.hasImage  ? Theme.statusReady : Theme.statusOff)
                            }

                            // ── Brush tab ──────────────────────────────────────
                            Rectangle {
                                width:  58; height: 24; radius: Theme.radiusSmall
                                color:  editorMode === 0
                                        ? Qt.rgba(0.85, 0.27, 0.27, 0.18) : Theme.bgLighter
                                border.color: editorMode === 0 ? Theme.maskColor : Theme.border
                                border.width: 1

                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text:           "Brush"
                                    color:          editorMode === 0 ? Theme.maskColor : Theme.textSecondary
                                    font.family:    Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.weight:    editorMode === 0 ? Font.Medium : Font.Normal
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked:    root.editorMode = 0
                                }
                            }

                            // ── SAM tab ────────────────────────────────────────
                            Rectangle {
                                width:  58; height: 24; radius: Theme.radiusSmall
                                color:  editorMode === 1
                                        ? Qt.rgba(0.29, 0.56, 0.85, 0.20) : Theme.bgLighter
                                border.color: editorMode === 1 ? "#4A90D9" : Theme.border
                                border.width: 1

                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text:           "SAM"
                                    color:          editorMode === 1 ? "#4A90D9" : Theme.textSecondary
                                    font.family:    Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.weight:    editorMode === 1 ? Font.Medium : Font.Normal
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked:    root.editorMode = 1
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // Mode hint / SAM status
                            Text {
                                visible:        editorMode === 0 && maskPainter.hasImage
                                text:           maskPainter.eraseMode ? "Erase mode" : "Draw mode"
                                color:          maskPainter.eraseMode ? "#64C8FF" : Theme.maskColor
                                font.family:    Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Text {
                                visible:        editorMode === 1
                                text:           samBridge.processing ? "Encoding..." :
                                                                       samBridge.ready      ? "SAM ready"  : "SAM not loaded"
                                color:          samBridge.processing ? Theme.statusBusy :
                                                                       samBridge.ready      ? Theme.statusReady : Theme.textDisabled
                                font.family:    Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }

                    // ── Brush canvas ───────────────────────────────────────────
                    MaskPainter {
                        id:                maskPainter
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        visible:           editorMode === 0
                        brushSize:         brushSizeSlider.value
                        eraseMode:         eraseModeBtn.checked
                        maskOpacity:       opacitySlider.value
                    }

                    // ── SAM canvas ─────────────────────────────────────────────
                    SegmentCanvas {
                        id:                segmentCanvas
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        visible:           editorMode === 1
                        maskOpacity:       samOpacitySlider.value
                        boxMode:           samInputMode === 1
                    }
                }
            }

            // ── Right: Result viewer ───────────────────────────────────────────
            Item {
                SplitView.fillWidth:    true
                SplitView.minimumWidth: 200

                ColumnLayout {
                    anchors.fill: parent
                    spacing:      0

                    Rectangle {
                        Layout.fillWidth: true
                        height:           28
                        color:            Theme.bgPanel

                        RowLayout {
                            anchors.fill:        parent
                            anchors.leftMargin:  Theme.marginNormal
                            anchors.rightMargin: Theme.marginNormal
                            spacing: 8

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: resultVersion > 0 ? Theme.statusReady : Theme.statusOff
                            }
                            Text {
                                text:           "Inpainted Result"
                                color:          Theme.textPrimary
                                font.family:    Theme.fontFamily
                                font.pixelSize: Theme.fontSizeNormal
                                font.weight:    Font.Medium
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                visible:        resultVersion > 0
                                text:           "v" + resultVersion
                                color:          Theme.textDisabled
                                font.family:    Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        color:             "#000000"

                        Image {
                            id:       resultImage
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            smooth:   true
                            cache:    false
                        }

                        Text {
                            anchors.centerIn: parent
                            visible:  resultVersion === 0 && !lamaBridge.processing
                            text:     "Run Inpainting\nto see the result"
                            color:    Theme.textDisabled
                            font.family:    Theme.fontFamily
                            font.pixelSize: Theme.fontSizeNormal
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Rectangle {
                            anchors.fill: parent
                            color:        "#CC000000"
                            visible:      lamaBridge.processing

                            Column {
                                anchors.centerIn: parent
                                spacing:          12

                                BusyIndicator {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    running: lamaBridge.processing
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text:           "Inpainting..."
                                    color:          Theme.textSecondary
                                    font.family:    Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeNormal
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Separator ──────────────────────────────────────────────────────────
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

        // ── Toolbar (52px — mode-dependent content) ────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height:           52
            color:            Theme.bgPanel

            // ── Brush toolbar ──────────────────────────────────────────────────
            RowLayout {
                visible:              editorMode === 0
                anchors.fill:         parent
                anchors.leftMargin:   Theme.marginLarge
                anchors.rightMargin:  Theme.marginLarge
                anchors.topMargin:    8
                anchors.bottomMargin: 8
                spacing:              Theme.marginNormal

                Text {
                    text:           "Mode:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Rectangle {
                    width: 68; height: 28; radius: Theme.radiusSmall
                    color:        !eraseModeBtn.checked ? Qt.rgba(0.85, 0.27, 0.27, 0.25) : Theme.bgLighter
                    border.color: !eraseModeBtn.checked ? Theme.maskColor : Theme.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text:           "Draw"
                        color:          !eraseModeBtn.checked ? Theme.maskColor : Theme.textSecondary
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight:    Font.Medium
                    }
                    MouseArea { anchors.fill: parent; onClicked: eraseModeBtn.checked = false }
                }

                Rectangle {
                    width: 68; height: 28; radius: Theme.radiusSmall
                    color:        eraseModeBtn.checked ? Qt.rgba(0.39, 0.78, 1.0, 0.20) : Theme.bgLighter
                    border.color: eraseModeBtn.checked ? "#64C8FF" : Theme.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text:           "Erase"
                        color:          eraseModeBtn.checked ? "#64C8FF" : Theme.textSecondary
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight:    Font.Medium
                    }
                    CheckBox {
                        id:          eraseModeBtn
                        anchors.fill: parent
                        checked:     false
                        contentItem: Item {}
                        indicator:   Item {}
                        background:  Item {}
                    }
                }

                StyledButton {
                    text:        "Clear"
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    pressColor:  Qt.darker(Theme.bgLight, 1.1)
                    enabled:     maskPainter.hasContent
                    onClicked:   maskPainter.clear()
                }

                Rectangle { width: 1; height: 24; color: Theme.divider }

                Text {
                    text:           "Brush:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Repeater {
                    model: [8, 20, 40, 70]
                    delegate: Rectangle {
                        width: 30; height: 28; radius: Theme.radiusSmall
                        color:        brushSizeSlider.value === modelData
                                      ? Qt.rgba(0.35, 0.61, 0.85, 0.30) : Theme.bgLighter
                        border.color: brushSizeSlider.value === modelData
                                      ? Theme.accent : Theme.border
                        border.width: 1

                        Item {
                            anchors.centerIn: parent
                            Rectangle {
                                anchors.centerIn: parent
                                width:  Math.max(4, modelData * 0.35)
                                height: width; radius: width / 2
                                color:  Theme.textSecondary
                            }
                        }
                        MouseArea { anchors.fill: parent; onClicked: brushSizeSlider.value = modelData }
                    }
                }

                Slider {
                    id:   brushSizeSlider
                    from: 4; to: 120; value: 25; stepSize: 1
                    Layout.preferredWidth: 90

                    background: Rectangle {
                        x: brushSizeSlider.leftPadding
                        y: brushSizeSlider.topPadding + brushSizeSlider.availableHeight / 2 - height / 2
                        width: brushSizeSlider.availableWidth; height: 4; radius: 2
                        color: Theme.bgLighter
                        Rectangle {
                            width:  brushSizeSlider.visualPosition * parent.width
                            height: parent.height; radius: 2; color: Theme.accent
                        }
                    }
                    handle: Rectangle {
                        x: brushSizeSlider.leftPadding + brushSizeSlider.visualPosition * brushSizeSlider.availableWidth - width / 2
                        y: brushSizeSlider.topPadding  + brushSizeSlider.availableHeight / 2 - height / 2
                        width: 14; height: 14; radius: 7
                        color: brushSizeSlider.pressed ? Theme.btnPress : Theme.accent
                    }
                }

                Text {
                    text:           brushSizeSlider.value + "px"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 30
                }

                Rectangle { width: 1; height: 24; color: Theme.divider }

                Text {
                    text:           "Opacity:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Slider {
                    id:   opacitySlider
                    from: 0.2; to: 1.0; value: 0.65; stepSize: 0.05
                    Layout.preferredWidth: 80

                    background: Rectangle {
                        x: opacitySlider.leftPadding
                        y: opacitySlider.topPadding + opacitySlider.availableHeight / 2 - height / 2
                        width: opacitySlider.availableWidth; height: 4; radius: 2
                        color: Theme.bgLighter
                        Rectangle {
                            width:  opacitySlider.visualPosition * parent.width
                            height: parent.height; radius: 2
                            color:  Qt.rgba(0.85, 0.27, 0.27, 0.9)
                        }
                    }
                    handle: Rectangle {
                        x: opacitySlider.leftPadding + opacitySlider.visualPosition * opacitySlider.availableWidth - width / 2
                        y: opacitySlider.topPadding  + opacitySlider.availableHeight / 2 - height / 2
                        width: 14; height: 14; radius: 7
                        color: opacitySlider.pressed ? Theme.btnPress : Theme.maskColor
                    }
                }

                Text {
                    text:           Math.round(opacitySlider.value * 100) + "%"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 32
                }
            }

            // ── SAM toolbar ────────────────────────────────────────────────────
            RowLayout {
                visible:              editorMode === 1
                anchors.fill:         parent
                anchors.leftMargin:   Theme.marginLarge
                anchors.rightMargin:  Theme.marginLarge
                anchors.topMargin:    8
                anchors.bottomMargin: 8
                spacing:              Theme.marginNormal

                // [Point] / [Box] sub-mode toggle
                Rectangle {
                    width: 54; height: 28; radius: Theme.radiusSmall
                    color:        samInputMode === 0 ? Qt.rgba(0.35, 0.61, 0.85, 0.30) : Theme.bgLighter
                    border.color: samInputMode === 0 ? Theme.accent : Theme.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text:           "Point"
                        color:          samInputMode === 0 ? Theme.accent : Theme.textSecondary
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight:    Font.Medium
                    }
                    MouseArea { anchors.fill: parent; onClicked: samInputMode = 0 }
                }

                Rectangle {
                    width: 54; height: 28; radius: Theme.radiusSmall
                    color:        samInputMode === 1 ? Qt.rgba(0.35, 0.61, 0.85, 0.30) : Theme.bgLighter
                    border.color: samInputMode === 1 ? Theme.accent : Theme.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text:           "Box"
                        color:          samInputMode === 1 ? Theme.accent : Theme.textSecondary
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight:    Font.Medium
                    }
                    MouseArea { anchors.fill: parent; onClicked: samInputMode = 1 }
                }

                Rectangle { width: 1; height: 24; color: Theme.divider }

                Text {
                    text:           samInputMode === 0
                                    ? "L-click: add  \u00B7  R-click: exclude"
                                    : "Drag to draw selection box"
                    color:          Theme.textDisabled
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Item { Layout.fillWidth: true }

                StyledButton {
                    text:        "Undo"
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    pressColor:  Qt.darker(Theme.bgLight, 1.1)
                    enabled:     samBridge.hasMask || samBridge.hasBox
                    onClicked: {
                        if (samInputMode === 0) {
                            samBridge.undoPoint()
                            segmentCanvas.undoLastPoint()
                        } else {
                            samBridge.clearBox()
                            segmentCanvas.clearBox()
                        }
                    }
                }

                StyledButton {
                    text:        "Clear"
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    pressColor:  Qt.darker(Theme.bgLight, 1.1)
                    enabled:     segmentCanvas.hasImage
                    onClicked: {
                        samBridge.clearPoints()
                        segmentCanvas.clear()
                    }
                }

                Rectangle { width: 1; height: 24; color: Theme.divider }

                Text {
                    text:           "Overlay:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Slider {
                    id:   samOpacitySlider
                    from: 0.1; to: 1.0; value: 0.50; stepSize: 0.05
                    Layout.preferredWidth: 80

                    background: Rectangle {
                        x: samOpacitySlider.leftPadding
                        y: samOpacitySlider.topPadding + samOpacitySlider.availableHeight / 2 - height / 2
                        width: samOpacitySlider.availableWidth; height: 4; radius: 2
                        color: Theme.bgLighter
                        Rectangle {
                            width:  samOpacitySlider.visualPosition * parent.width
                            height: parent.height; radius: 2; color: "#4A90D9"
                        }
                    }
                    handle: Rectangle {
                        x: samOpacitySlider.leftPadding + samOpacitySlider.visualPosition * samOpacitySlider.availableWidth - width / 2
                        y: samOpacitySlider.topPadding  + samOpacitySlider.availableHeight / 2 - height / 2
                        width: 14; height: 14; radius: 7
                        color: samOpacitySlider.pressed ? Theme.btnPress : "#4A90D9"
                    }
                }

                Text {
                    text:           Math.round(samOpacitySlider.value * 100) + "%"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 32
                }

                Rectangle { width: 1; height: 24; color: Theme.divider }

                Text {
                    text:           "Expand:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                Slider {
                    id:   dilateSlider
                    from: 0; to: 80; value: samBridge.dilateRadius; stepSize: 2
                    Layout.preferredWidth: 90

                    onValueChanged: samBridge.dilateRadius = value

                    background: Rectangle {
                        x: dilateSlider.leftPadding
                        y: dilateSlider.topPadding + dilateSlider.availableHeight / 2 - height / 2
                        width: dilateSlider.availableWidth; height: 4; radius: 2
                        color: Theme.bgLighter
                        Rectangle {
                            width:  dilateSlider.visualPosition * parent.width
                            height: parent.height; radius: 2; color: "#4A90D9"
                        }
                    }
                    handle: Rectangle {
                        x: dilateSlider.leftPadding + dilateSlider.visualPosition * dilateSlider.availableWidth - width / 2
                        y: dilateSlider.topPadding  + dilateSlider.availableHeight / 2 - height / 2
                        width: 14; height: 14; radius: 7
                        color: dilateSlider.pressed ? Theme.btnPress : "#4A90D9"
                    }
                }

                Text {
                    text:           dilateSlider.value + "px"
                    color:          dilateSlider.value > 0 ? Theme.textSecondary : Theme.textDisabled
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                    Layout.preferredWidth: 28
                }
            }
        }

        // ── SAM model bar (visible in SAM mode only) ───────────────────────────
        Rectangle {
            visible:          editorMode === 1
            Layout.fillWidth: true
            height:           1
            color:            Theme.divider
        }

        Rectangle {
            visible:          editorMode === 1
            Layout.fillWidth: true
            height:           52
            color:            Theme.bgMedium

            RowLayout {
                anchors.fill:         parent
                anchors.leftMargin:   Theme.marginLarge
                anchors.rightMargin:  Theme.marginLarge
                anchors.topMargin:    8
                anchors.bottomMargin: 8
                spacing:              Theme.marginNormal

                Text {
                    text:           "SAM:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                TextField {
                    id:              samModelDirField
                    Layout.fillWidth: true
                    color:           Theme.textPrimary
                    font.family:     Theme.fontFamily
                    font.pixelSize:  Theme.fontSizeSmall
                    leftPadding:     8

                    background: Rectangle {
                        color:        Theme.bgInput
                        radius:       Theme.radiusSmall
                        border.color: samModelDirField.activeFocus ? Theme.borderFocus : Theme.border
                        border.width: 1
                    }

                    onTextChanged: appSettings.samModelDir = text
                }

                StyledButton {
                    text:        "..."
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    pressColor:  Qt.darker(Theme.bgLight, 1.1)
                    implicitWidth: 36
                    onClicked:   samBrowseDialog.open()
                }

                ComboBox {
                    id:            samVariantCombo
                    model:         ["Tiny", "Small", "Base+", "Large"]
                    currentIndex:  1
                    implicitWidth: 76
                    onCurrentIndexChanged: appSettings.samVariantIndex = currentIndex

                    contentItem: Text {
                        leftPadding:       8
                        text:              samVariantCombo.displayText
                        color:             Theme.textPrimary
                        font.family:       Theme.fontFamily
                        font.pixelSize:    Theme.fontSizeSmall
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color:        Theme.bgInput
                        radius:       Theme.radiusSmall
                        border.color: samVariantCombo.activeFocus ? Theme.borderFocus : Theme.border
                        border.width: 1
                    }
                }

                StyledButton {
                    text:        samBridge.ready ? "Reload" : "Load SAM"
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    pressColor:  Qt.darker(Theme.bgLight, 1.1)
                    enabled:     !samBridge.processing && samModelDirField.text.trim().length > 0
                    onClicked: {
                        var idx     = samVariantCombo.currentIndex
                        var v       = root.samVariants[idx]
                        var dir     = samModelDirField.text.trim()
                        var enc     = dir + "/" + v.folder + "/sam2_hiera_" + v.fileId + ".encoder.onnx"
                        var dec     = dir + "/" + v.folder + "/sam2_hiera_" + v.fileId + ".decoder.onnx"
                        root.samStatusOverride = ""
                        samBridge.initialize(enc, dec)
                    }
                }

                Rectangle { width: 1; height: 28; color: Theme.divider }

                // SAM status indicator
                Row {
                    spacing: 5

                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: samBridge.processing ? Theme.statusBusy :
                                                      samBridge.ready      ? Theme.statusReady : Theme.statusOff

                        SequentialAnimation on opacity {
                            running: samBridge.processing
                            loops:   Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 600 }
                            NumberAnimation { to: 1.0; duration: 600 }
                        }
                    }

                    Text {
                        id: samStatusText
                        anchors.verticalCenter: parent.verticalCenter
                        text:  samBridge.processing         ? "Processing..." :
                                                              samBridge.ready              ? "SAM ready"     :
                                                                                             root.samStatusOverride !== "" ? root.samStatusOverride :
                                                                                                                             "SAM not loaded"
                        color: samBridge.processing ? Theme.statusBusy :
                                                      samBridge.ready      ? Theme.statusReady : Theme.textDisabled
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }

        // ── Separator ──────────────────────────────────────────────────────────
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.divider }

        // ── Inpaint control bar ────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height:           58
            color:            Theme.bgMedium

            RowLayout {
                anchors.fill:         parent
                anchors.leftMargin:   Theme.marginLarge
                anchors.rightMargin:  Theme.marginLarge
                anchors.topMargin:    8
                anchors.bottomMargin: 8
                spacing:              Theme.marginNormal

                Text {
                    text:           "Model:"
                    color:          Theme.textSecondary
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }

                TextField {
                    id:              modelPathField
                    Layout.fillWidth: true
                    text:            lamaBridge.modelPath
                    color:           Theme.textPrimary
                    font.family:     Theme.fontFamily
                    font.pixelSize:  Theme.fontSizeSmall
                    leftPadding:     8

                    background: Rectangle {
                        color:        Theme.bgInput
                        radius:       Theme.radiusSmall
                        border.color: modelPathField.activeFocus ? Theme.borderFocus : Theme.border
                        border.width: 1
                    }

                    onTextChanged: {
                        if (text !== lamaBridge.modelPath) lamaBridge.modelPath = text
                        appSettings.lamaModelPath = text
                    }
                }

                StyledButton {
                    text:        "..."
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    pressColor:  Qt.darker(Theme.bgLight, 1.1)
                    implicitWidth: 36
                    onClicked:   lamaBrowseDialog.open()
                }

                StyledButton {
                    text:        lamaBridge.ready ? "Reload" : "Load Model"
                    compact:     true
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    enabled:     !lamaBridge.processing
                    onClicked:   lamaBridge.initialize(modelPathField.text.trim())
                }

                Rectangle { width: 1; height: 28; color: Theme.divider }

                Row {
                    spacing: 5

                    Rectangle {
                        width: 8; height: 8; radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: lamaBridge.processing ? Theme.statusBusy :
                                                       lamaBridge.ready      ? Theme.statusReady : Theme.statusOff

                        SequentialAnimation on opacity {
                            running: lamaBridge.processing
                            loops:   Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 600 }
                            NumberAnimation { to: 1.0; duration: 600 }
                        }
                    }

                    Text {
                        id: statusText
                        anchors.verticalCenter: parent.verticalCenter
                        text:  lamaBridge.processing ? "Processing..." :
                                                       lamaBridge.ready      ? "Model ready"   : "Model not loaded"
                        color: lamaBridge.processing ? Theme.statusBusy :
                                                       lamaBridge.ready      ? Theme.statusReady : Theme.textDisabled
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }

                Item { Layout.preferredWidth: Theme.marginSmall }

                // ── Run Inpainting ─────────────────────────────────────────────
                StyledButton {
                    text:          "\u25B6  Run Inpainting"
                    implicitWidth: 160
                    implicitHeight: 36
                    normalColor:   Theme.btnSuccess
                    hoverColor:    Qt.lighter(Theme.btnSuccess, 1.15)
                    pressColor:    Qt.darker(Theme.btnSuccess,  1.1)
                    enabled: lamaBridge.ready && !lamaBridge.processing &&
                             (editorMode === 0 ? maskPainter.hasContent : samBridge.hasMask)
                    onClicked: {
                        var imgB64, maskB64
                        if (editorMode === 0) {
                            imgB64  = maskPainter.getImageBase64()
                            maskB64 = maskPainter.getMaskBase64()
                        } else {
                            imgB64  = segmentCanvas.getImageBase64()
                            maskB64 = samBridge.getMaskBase64()
                        }
                        if (imgB64.length > 0 && maskB64.length > 0)
                            lamaBridge.inpaint(imgB64, maskB64)
                    }
                }

                // ── Save Result ────────────────────────────────────────────────
                StyledButton {
                    text:        "Save Result"
                    implicitHeight: 36
                    compact:     true
                    enabled:     resultVersion > 0 && !lamaBridge.processing
                    normalColor: Theme.bgLighter
                    hoverColor:  Theme.bgLight
                    onClicked:   saveDialog.open()
                }
            }
        }
    }

    // ── Persistent settings ───────────────────────────────────────────────────
    Settings {
        id: appSettings
        property string lamaModelPath:   "D:/CTEC/Project/Qt/LaMaQt/models/LaMa/lama.onnx"
        property string samModelDir:     "D:/CTEC/Project/Qt/LaMaQt/models/SAM_2"
        property int    samVariantIndex: 1   // Small
    }

    // ── Auto-load models on startup ───────────────────────────────────────────
    Component.onCompleted: {
        var lamaPath = appSettings.lamaModelPath
        var samDir   = appSettings.samModelDir

        // Restore paths to UI fields
        if (lamaPath.length > 0)
            lamaBridge.modelPath = lamaPath

        samModelDirField.text        = samDir
        samVariantCombo.currentIndex = appSettings.samVariantIndex

        // Auto-initialize LaMa
        if (lamaPath.length > 0)
            lamaBridge.initialize(lamaPath)

        // Auto-initialize SAM
        if (samDir.length > 0) {
            var v = root.samVariants[appSettings.samVariantIndex]
            samBridge.initialize(
                samDir + "/" + v.folder + "/sam2_hiera_" + v.fileId + ".encoder.onnx",
                samDir + "/" + v.folder + "/sam2_hiera_" + v.fileId + ".decoder.onnx"
            )
        }
    }

    // ── Connections: SegmentCanvas → SAMBridge ────────────────────────────────
    Connections {
        target: segmentCanvas

        function onPointAdded(x, y, isPositive) {
            samBridge.addPoint(x, y, isPositive)
        }

        function onBoxSet(x1, y1, x2, y2) {
            samBridge.setBox(x1, y1, x2, y2)
        }
    }

    // ── Connections: SAMBridge signals ────────────────────────────────────────
    Connections {
        target: samBridge

        function onMaskReady(maskBase64) {
            segmentCanvas.setMaskOverlay(maskBase64)
        }

        function onInitializeDone(success) {
            if (success) {
                root.samStatusOverride = ""
                // If an image was loaded before SAM was ready, encode it now
                if (root.currentImageBase64.length > 0)
                    samBridge.setImage(root.currentImageBase64)
            } else {
                root.samStatusOverride = "Load failed"
            }
        }

        function onErrorOccurred(msg) {
            root.samStatusOverride = msg
        }
    }

    // ── Connections: LaMaBridge signals ───────────────────────────────────────
    Connections {
        target: lamaBridge

        function onInpaintCompleted(version) {
            root.resultVersion = version
            resultImage.source = ""
            resultImage.source = "image://result/frame?v=" + version
            statusText.text    = "Inpainting done  (v" + version + ")"
            statusText.color   = Theme.statusReady
        }

        function onErrorOccurred(msg) {
            statusText.text  = "Error: " + msg
            statusText.color = Theme.statusError
        }

        function onInitializeDone(success) {
            if (success) {
                statusText.text  = "Model loaded"
                statusText.color = Theme.statusReady
            }
        }
    }

    // ── LaMa model file picker ────────────────────────────────────────────────
    FileDialog {
        id:          lamaBrowseDialog
        title:       "Select LaMa ONNX Model"
        fileMode:    FileDialog.OpenFile
        nameFilters: ["ONNX model (*.onnx)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            modelPathField.text = path
        }
    }

    // ── SAM model folder picker ───────────────────────────────────────────────
    FolderDialog {
        id:    samBrowseDialog
        title: "Select SAM Model Directory"
        onAccepted: {
            var path = selectedFolder.toString().replace("file:///", "")
            samModelDirField.text = path
        }
    }

    // ── Save dialog ────────────────────────────────────────────────────────────
    FileDialog {
        id:            saveDialog
        title:         "Save Inpainted Result"
        fileMode:      FileDialog.SaveFile
        nameFilters:   ["PNG image (*.png)", "JPEG image (*.jpg)"]
        defaultSuffix: "png"
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            saveCanvas.saveToPath(path)
        }
    }

    // Canvas for saving result image
    Canvas {
        id:      saveCanvas
        visible: false
        width:   resultImage.implicitWidth  > 0 ? resultImage.implicitWidth  : 1
        height:  resultImage.implicitHeight > 0 ? resultImage.implicitHeight : 1

        property string savePath: ""

        function saveToPath(path) {
            savePath = path
            if (isImageLoaded(resultImage.source))
                requestPaint()
            else
                loadImage(resultImage.source)
        }

        onImageLoaded: { requestPaint() }

        onPaint: {
            if (savePath.length === 0) return
            var ctx = getContext("2d")
            ctx.drawImage(resultImage.source, 0, 0, width, height)
            save(savePath)
            savePath = ""
        }
    }
}
