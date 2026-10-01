pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Holonight.Core
import Holonight.Controls
import HolonightViewer

Rectangle {
    id: strip
    required property ImageDocument document
    required property real magnification
    required property bool shown
    objectName: "detailsStrip"
    width: Math.min(parent.width - 24, stripFilename.implicitWidth + metadata.naturalWidth + 36)
    height: detailsFlow.height + 16
    radius: HnMetrics.internalSpacing(HnControlSize.Compact)
    color: Qt.alpha(HoloniightPalette.surface, 0.85)
    border.color: HoloniightPalette.borderPassive
    opacity: shown ? 1 : 0
    visible: shown || opacity > 0
    Behavior on opacity {
        NumberAnimation {
            duration: 150
        }
    }
    Flow {
        id: detailsFlow
        x: 12
        y: 8
        width: parent.width - 24
        spacing: 12
        HnLabel {
            id: stripFilename
            width: Math.min(implicitWidth, Math.max(80, detailsFlow.width - metadata.naturalWidth - 12))
            rawText: strip.document.fileName
            elide: Text.ElideMiddle
            textFormat: Text.PlainText
        }
        Flow {
            id: metadata
            readonly property real naturalWidth: {
                let total = Math.max(0, metadataSections.count - 1) * spacing;
                for (let i = 0; i < metadataSections.count; ++i) {
                    const section = metadataSections.itemAt(i);
                    if (section)
                        total += section.implicitWidth;
                }
                return total;
            }
            width: Math.min(naturalWidth, detailsFlow.width)
            spacing: 12
            Repeater {
                id: metadataSections
                model: {
                    const sections = [];
                    sections.push(qsTranslate("Main", "%1 × %2").arg(strip.document.transformedDimensions.width).arg(strip.document.transformedDimensions.height));
                    sections.push(strip.document.formattedFileSize);
                    sections.push(qsTranslate("Main", "%1%").arg(Number(strip.magnification * 100).toLocaleString(Qt.locale(), 'f', strip.magnification < 0.01 ? 4 : 1)));
                    sections.push(qsTranslate("Main", "%1 / %2").arg(strip.document.position).arg(strip.document.count));
                    if (strip.document.animation.animated) {
                        sections.push(qsTranslate("Main", "Animated"), strip.document.animation.playing ? qsTranslate("Main", "Playing") : qsTranslate("Main", "Paused"));
                        if (strip.document.animation.frameCount > 0)
                            sections.push(qsTranslate("Main", "%1 frames").arg(strip.document.animation.frameCount));
                    }
                    return sections;
                }
                RowLayout {
                    id: section
                    required property string modelData
                    width: Math.min(implicitWidth, metadata.width)
                    spacing: 12
                    HnSeparator {
                        orientation: Qt.Vertical
                        Layout.fillHeight: true
                    }
                    HnLabel {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        rawText: section.modelData
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.StaticText
                        Accessible.name: rawText
                    }
                }
            }
        }
        HnLabel {
            objectName: "playbackNotice"
            width: detailsFlow.width
            visible: strip.document.animation.failureNotice.length > 0
            color: HoloniightPalette.warning
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            rawText: strip.document.animation.failureNotice
            Accessible.role: Accessible.StaticText
            Accessible.name: rawText
        }
    }
}
