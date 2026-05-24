/*
 * Harness scene for §2.1 WelcomeScreen.
 */
import QtQuick
import SlmInstaller 1.0

WelcomeScreen {
    width: 960
    height: 720

    onContinueRequested: {
        console.log("[harness] continueRequested")
        Qt.quit()
    }
}
