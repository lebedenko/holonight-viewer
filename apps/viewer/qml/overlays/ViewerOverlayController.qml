pragma ComponentBehavior: Bound

import QtQuick

QtObject {
    id: controller
    required property bool fullscreen
    required property bool controlsHovered
    required property bool menuOpen
    required property bool imageReady
    // Transient overlays: the flags are the display target, the timers only end a reveal.
    property bool arrowsShown: false
    property bool countdownActive: false
    property bool detailsShown: false
    property Timer arrowCountdown: Timer {
        id: arrowTimer
        objectName: "arrowTimer"
        interval: 3000
        running: controller.countdownActive && !controller.controlsHovered && !controller.menuOpen
        onTriggered: {
            controller.arrowsShown = false;
            controller.countdownActive = false;
        }
    }
    property Timer detailsCountdown: Timer {
        id: detailsTimer
        objectName: "detailsTimer"
        interval: 4000
        onTriggered: controller.detailsShown = false
    }
    function showArrows(): void {
        controller.arrowsShown = true;
        controller.countdownActive = true;
        if (arrowTimer.running)
            arrowTimer.restart();
    }
    function hideArrows(): void {
        controller.arrowsShown = false;
        if (!controller.fullscreen)
            controller.countdownActive = false;
    }
    onFullscreenChanged: {
        if (fullscreen)
            showArrows();
        else {
            arrowsShown = false;
            countdownActive = false;
        }
    }
    function revealDetails(): void {
        if (!controller.imageReady)
            return;
        controller.detailsShown = true;
        detailsTimer.restart();
    }
    function concealDetails(): void {
        controller.detailsShown = false;
        detailsTimer.stop();
    }
}
