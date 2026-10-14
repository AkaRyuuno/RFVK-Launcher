import QtQuick
import Rfvk

// Текст с готовыми ролями из типографики дизайна. Пример: AppText { role: "display" }.
Text {
    id: t
    property string role: "body"

    color: Theme.text
    renderType: Text.NativeRendering
    font.family: role === "mono" ? Theme.fontMono
               : (role === "display" || role === "title" || role === "h2" || role === "h3") ? Theme.fontH
               : Theme.fontUI

    font.pixelSize: {
        switch (role) {
        case "display": return 26
        case "title": return 20
        case "h2": return 19
        case "h3": return 17
        case "bodyStrong":
        case "body": return 14
        case "small": return 13
        case "overline":
        case "caption": return 11
        case "mono": return 13
        default: return 14
        }
    }
    font.weight: {
        switch (role) {
        case "display":
        case "title":
        case "h2":
        case "h3": return Font.Bold
        case "bodyStrong":
        case "overline": return Font.DemiBold
        default: return Font.Normal
        }
    }
    lineHeight: {
        switch (role) {
        case "display": return 32 / 26
        case "title": return 26 / 20
        case "h2": return 24 / 19
        case "h3": return 22 / 17
        case "body":
        case "bodyStrong": return 20 / 14
        case "small": return 18 / 13
        default: return 1.3
        }
    }
    lineHeightMode: Text.ProportionalHeight
    font.capitalization: role === "overline" ? Font.AllUppercase : Font.MixedCase
    font.letterSpacing: role === "overline" ? font.pixelSize * 0.08 : 0
}
