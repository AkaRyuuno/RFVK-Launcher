import QtQuick
import Rfvk

AppText {
    id: label

    lineHeight: 1
    verticalAlignment: Text.AlignVCenter

    readonly property int centerShift: {
        // Один образец на шрифт сохраняет базовую линию при смене подписи.
        // Нижние выносные элементы занимают меньшую часть строки.
        const ink = metrics.tightBoundingRect("Нр")
        const aboveBaseline = -ink.y
        const belowBaseline = ink.y + ink.height
        return Math.round(metrics.height / 2 - metrics.ascent + aboveBaseline / 2 - belowBaseline / 4)
    }
    topPadding: Math.max(0, 2 * centerShift)
    bottomPadding: Math.max(0, -2 * centerShift)

    FontMetrics {
        id: metrics
        font: label.font
    }
}
