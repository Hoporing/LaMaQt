import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: root

    property color normalColor: Theme.btnNormal
    property color hoverColor:  Theme.btnHover
    property color pressColor:  Theme.btnPress
    property bool  compact:     false

    implicitHeight: compact ? 26 : 32
    leftPadding:  compact ? 8 : 12
    rightPadding: compact ? 8 : 12

    contentItem: Text {
        text:            root.text
        font.family:     Theme.fontFamily
        font.pixelSize:  compact ? Theme.fontSizeSmall : Theme.fontSizeNormal
        font.weight:     Font.Medium
        color:           root.enabled ? Theme.textPrimary : Theme.textDisabled
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment:   Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: {
            if (!root.enabled)    return Theme.bgLighter
            if (root.pressed)     return root.pressColor
            if (root.hovered)     return root.hoverColor
            return root.normalColor
        }
        border.color: root.hovered && root.enabled ? Qt.lighter(root.normalColor, 1.3) : "transparent"
        border.width: 1

        Behavior on color { ColorAnimation { duration: 80 } }
    }
}
