pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls

Controls.BusyIndicator {
    id: indicator
    objectName: "viewerLoadingIndicator"
    required property bool loading
    property bool suppressed: false
    opacity: 0
    visible: opacity > 0 && !suppressed
    running: visible
    focusPolicy: Qt.NoFocus
    Accessible.name: qsTr("Loading image")
    Accessible.ignored: !loading || !visible

    function fadeTo(target: real): void {
        fade.stop();
        fade.to = target;
        fade.duration = target > 0 ? 100 : 80;
        fade.start();
    }
    function updateLoading(): void {
        delay.stop();
        if (loading) {
            fade.stop();
            opacity = 0;
            delay.start();
        } else {
            fadeTo(0);
        }
    }
    onLoadingChanged: updateLoading()
    onSuppressedChanged: {
        if (suppressed) {
            delay.stop();
            fade.stop();
            opacity = 0;
        }
    }
    Component.onCompleted: updateLoading()

    Timer {
        id: delay
        interval: 200
        repeat: false
        onTriggered: {
            if (indicator.loading && !indicator.suppressed)
                indicator.fadeTo(1);
        }
    }
    NumberAnimation {
        id: fade
        target: indicator
        property: "opacity"
    }
}
