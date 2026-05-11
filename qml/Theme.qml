pragma Singleton
import QtQuick 2.15

QtObject {
    // ── Backgrounds ────────────────────────────────────────────────
    readonly property color bgDark:      "#1E1E1E"
    readonly property color bgMedium:    "#252525"
    readonly property color bgPanel:     "#2B2B2B"
    readonly property color bgLight:     "#333333"
    readonly property color bgLighter:   "#3C3C3C"
    readonly property color bgInput:     "#1A1A1A"

    // ── Text ───────────────────────────────────────────────────────
    readonly property color textPrimary:   "#FFFFFF"
    readonly property color textSecondary: "#BBBBBB"
    readonly property color textDisabled:  "#606060"
    readonly property color textHint:      "#808080"

    // ── Buttons & Accents ──────────────────────────────────────────
    readonly property color btnNormal:  "#367BEB"
    readonly property color btnHover:   "#5CBCE6"
    readonly property color btnPress:   "#4A9ED6"
    readonly property color btnDanger:  "#C0392B"
    readonly property color btnSuccess: "#27AE60"
    readonly property color btnGreen:   "#2ECC71"
    readonly property color accent:     "#5B9BD5"

    // ── Status colors ──────────────────────────────────────────────
    readonly property color statusReady:   "#2ECC71"
    readonly property color statusBusy:    "#F39C12"
    readonly property color statusError:   "#E74C3C"
    readonly property color statusOff:     "#606060"

    // ── Borders & Dividers ─────────────────────────────────────────
    readonly property color border:        "#404040"
    readonly property color borderFocus:   "#5B9BD5"
    readonly property color divider:       "#383838"

    // ── Mask overlay color (used as reference) ─────────────────────
    readonly property color maskColor:     "#FF4646"

    // ── Typography ─────────────────────────────────────────────────
    readonly property string fontFamily:   "Segoe UI"
    readonly property int fontSizeSmall:   10
    readonly property int fontSizeNormal:  12
    readonly property int fontSizeMedium:  13
    readonly property int fontSizeLarge:   15
    readonly property int fontSizeTitle:   17

    // ── Layout metrics ─────────────────────────────────────────────
    readonly property int radiusSmall:   4
    readonly property int radiusNormal:  6
    readonly property int radiusLarge:   10
    readonly property int marginSmall:   6
    readonly property int marginNormal:  10
    readonly property int marginLarge:   16
    readonly property int menuBarHeight: 28
    readonly property int menuItemHeight: 28
    readonly property int toolbarHeight: 34
    readonly property int labelHeight:   22
}
