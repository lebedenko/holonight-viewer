pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import HolonightViewer

Item {
    id: root
    objectName: "thumbnailGrid"

    required property var model
    // Bound to document.scanning by Main.
    property bool scanning: false
    // Changes on a rescan so thumbnails decode again.
    property int generation: 0
    property real devicePixelRatio: 1
    property real cellPadding: 8
    property real labelGap: 6
    property real cellSpacing: 4
    readonly property real cellWidth: ThumbnailMetrics.boxSize + 2 * cellPadding + cellSpacing
    readonly property real cellHeight: cellPadding + ThumbnailMetrics.boxSize + labelGap + labelProbe.implicitHeight + cellPadding + cellSpacing
    // Taken from the width this item is given, never from the view, so laying out cannot resize its own input.
    readonly property int columns: Math.max(1, Math.floor(width / cellWidth))
    readonly property int visibleRows: height > 0 ? Math.max(1, Math.floor(height / cellHeight)) : 1
    readonly property int count: view.count
    property url selectedUrl
    property int selectedIndex: -1
    // The last valid index, used to fall back to when the selected file disappears on a rescan.
    property int lastIndex: 0
    readonly property bool showsEmpty: !scanning && count === 0

    signal activated(url fileUrl)
    // A click chose a cell; Main drops any header-button focus so a following Enter opens the file.
    signal selectionInteraction

    function scrollTo(mode: int): void {
        if (root.selectedIndex >= 0 && root.count > 0) {
            view.positionViewAtIndex(root.selectedIndex, mode);
        }
    }

    function scrollIntoView(): void {
        root.scrollTo(GridView.Contain);
    }

    function chooseIndex(index: int): void {
        root.selectedIndex = index;
        root.selectedUrl = root.model.urlAt(index);
        root.lastIndex = index;
    }

    // Selects the open file, or the first one when it is not listed, and centres it once layout has a size.
    function enter(url: url): void {
        if (root.count === 0) {
            root.selectedUrl = url;
            root.selectedIndex = -1;
            return;
        }
        const index = root.model.indexOfUrl(url);
        root.chooseIndex(index < 0 ? 0 : index);
        Qt.callLater(root.scrollTo, GridView.Center);
    }

    function select(index: int): void {
        if (root.scanning || index < 0 || index >= root.count) {
            return;
        }
        root.chooseIndex(index);
        root.scrollIntoView();
    }

    function target(move: int): int {
        return GridNavigation.target(root.selectedIndex, move, root.count, root.columns, root.visibleRows);
    }

    function canMove(move: int): bool {
        if (root.scanning)
            return false;
        const index = root.target(move);
        return index >= 0 && index !== root.selectedIndex;
    }

    function move(move: int): void {
        if (root.canMove(move)) {
            root.select(root.target(move));
        }
    }

    function activateSelection(): void {
        if (!root.scanning && root.selectedIndex >= 0 && root.selectedIndex < root.count) {
            root.activated(root.selectedUrl);
        }
    }

    // After a rescan: keep the same file, or the item at the old position when it is gone.
    function restoreSelection(): void {
        if (root.count === 0) {
            root.selectedIndex = -1;
            return;
        }
        let index = root.model.indexOfUrl(root.selectedUrl);
        if (index < 0) {
            index = Math.max(0, Math.min(root.lastIndex, root.count - 1));
        }
        root.chooseIndex(index);
        Qt.callLater(root.scrollTo, GridView.Contain);
    }

    onScanningChanged: {
        if (!scanning) {
            // The deferral swallows the transient false of a rescan started during a scan.
            Qt.callLater(() => {
                if (!root.scanning) {
                    root.restoreSelection();
                }
            });
        }
    }

    // Grows the viewport-dependent scroll position once the first real size arrives.
    onWidthChanged: Qt.callLater(root.scrollIntoView)
    onHeightChanged: Qt.callLater(root.scrollIntoView)

    HnLabel {
        id: labelProbe
        visible: false
        rawText: "Ag"
    }

    GridView {
        id: view
        objectName: "thumbnailGridView"
        anchors.fill: parent
        visible: !root.scanning
        // Outside grid mode no delegates exist, so nothing is requested from the decoder.
        model: root.visible ? root.model : null
        cellWidth: root.cellWidth
        cellHeight: root.cellHeight
        // Centres the columns. The margins leave room for exactly `columns` cells, so the view lays out the same count.
        leftMargin: Math.max(0, Math.floor((root.width - root.columns * root.cellWidth) / 2))
        rightMargin: leftMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: false
        activeFocusOnTab: false
        reuseItems: false
        // Anchor layout can briefly produce a negative height while the window changes state.
        cacheBuffer: Math.max(0, height)
        Controls.ScrollBar.vertical: Controls.ScrollBar {}
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Images")

        delegate: ThumbnailCell {
            id: cell
            width: root.cellWidth
            height: root.cellHeight
            padding: root.cellPadding
            spacing: root.cellSpacing
            labelGap: root.labelGap
            isSelected: cell.url === root.selectedUrl
            source: ThumbnailMetrics.sourceFor(cell.url, root.devicePixelRatio, root.generation)
            onClicked: {
                root.select(cell.index);
                root.selectionInteraction();
            }
            onDoubleClicked: {
                root.select(cell.index);
                root.selectionInteraction();
                root.activateSelection();
            }
        }
    }

    HnLoadingState {
        objectName: "gridBusy"
        anchors.centerIn: parent
        width: Math.min(root.width - 32, 320)
        visible: root.scanning
        titleText: qsTr("Scanning folder…")
    }
}
