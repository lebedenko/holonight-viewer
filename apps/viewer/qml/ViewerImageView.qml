pragma ComponentBehavior: Bound

import QtQuick

ImageCanvas {
    id: root
    required property int inputEpoch
    required property bool canInspect
    signal focusRequested
    signal detailsRequested
    DragHandler {
        id: drag
        target: null
        enabled: root.canInspect
        acceptedButtons: Qt.LeftButton
        property int gestureEpoch: -1
        readonly property bool validGesture: active && gestureEpoch === root.inputEpoch
        cursorShape: validGesture && root.canPan ? Qt.ClosedHandCursor : root.canPan ? Qt.OpenHandCursor : Qt.ArrowCursor
        onActiveChanged: {
            gestureEpoch = active ? root.inputEpoch : -1;
            if (active)
                root.focusRequested();
        }
        onTranslationChanged: delta => {
            if (drag.validGesture)
                root.pan(Qt.point(delta.x, delta.y));
        }
    }
    WheelHandler {
        target: null
        enabled: root.canInspect
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            const steps = event.pixelDelta.y !== 0 ? event.pixelDelta.y / 40 : event.angleDelta.y / 120;
            if (steps !== 0) {
                root.zoomSteps(steps, Qt.point(event.x, event.y));
                root.detailsRequested();
                root.focusRequested();
                event.accepted = true;
            } else {
                event.accepted = false;
            }
        }
    }
}
