import QtQuick 2.12
import QtQuick.Window 2.12
import Lomiri.Components 1.3
import QtSensors 5.11
import CruelFury 1.0

Window {
    id: rootWin
    visible: true
    width: units.gu(45)
    height: units.gu(80)
    title: "Cruel Fury 3D"
    color: "#090412"

    property bool forceLandscape: true
    property bool showTitleScreen: true
    property int touchHudMode: 0 // 0=Auto, 1=Always On, 2=Always Off
    readonly property bool showTouchControls: touchHudMode === 1 || (touchHudMode === 0 && !game.desktopInputActive)

    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state !== Qt.ApplicationActive) {
                game.clearAllInputs()
                if (!showTitleScreen) {
                    game.paused = true
                }
            }
        }
    }

    Loader {
        id: gyroLoader
        active: game.gyroEnabled && !rootWin.showTitleScreen && !game.paused
        sourceComponent: Component {
            Gyroscope {
                active: true
                dataRate: 60
                onReadingChanged: {
                    if (reading) {
                        game.addGyroRates(reading.x, reading.y, 1.0 / 60.0)
                    }
                }
            }
        }
    }

    MainView {
        id: lomiriMain
        anchors.fill: parent
        applicationName: "cruelfury.plm"
        theme.name: "Lomiri.Components.Themes.SuruDark"
        backgroundColor: "#090412"

        // Rotatable stage so Landscape Widescreen works even when Ubuntu Touch portrait rotation-lock is ON!
        Item {
            id: stage
            readonly property bool rotate90: rootWin.forceLandscape && (lomiriMain.height > lomiriMain.width)
            width: rotate90 ? lomiriMain.height : lomiriMain.width
            height: rotate90 ? lomiriMain.width : lomiriMain.height
            anchors.centerIn: parent
            rotation: rotate90 ? 90 : 0
            focus: true

            Connections {
                target: game
                onPausedChanged: {
                    if (!game.paused && !rootWin.showTitleScreen) {
                        stage.forceActiveFocus()
                    }
                }
            }

            // Full keyboard support (WASD / Arrows / Space / Q / F / E / R / 1-4 / Ctrl / Tab / Esc)
            Keys.onPressed: {
                if (event.isAutoRepeat) return
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_P) {
                    if (!rootWin.showTitleScreen) game.paused = !game.paused
                    event.accepted = true
                } else if (event.key === Qt.Key_Tab) {
                    rootWin.touchHudMode = (rootWin.touchHudMode + 1) % 3
                    event.accepted = true
                } else {
                    game.handleKeyEvent(event.key, true)
                    event.accepted = true
                }
            }

            Keys.onReleased: {
                if (event.isAutoRepeat) return
                game.handleKeyEvent(event.key, false)
                event.accepted = true
            }

            // === HARDWARE OPENGL ES 2.0 3D ENGINE VIEWPORT ===
            GameViewport {
                id: game
                anchors.fill: parent

                onHapticPulse: {
                    Haptics.play()
                }
            }

            // === CENTER DYNAMIC CROSSHAIR ===
            Item {
                id: crosshair
                anchors.centerIn: parent
                visible: !rootWin.showTitleScreen && !game.paused && !game.isGameOver && !game.isVictory
                width: units.gu(3.5)
                height: units.gu(3.5)

                readonly property color retCol: game.crosshairOnTarget ? "#ff2bd6" : "#28f7ff"

                Rectangle {
                    anchors.centerIn: parent
                    width: units.dp(4); height: units.dp(4); radius: units.dp(2)
                    color: crosshair.retCol
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 0; width: units.dp(2); height: units.gu(1)
                    color: crosshair.retCol
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom; width: units.dp(2); height: units.gu(1)
                    color: crosshair.retCol
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 0; width: units.gu(1); height: units.dp(2)
                    color: crosshair.retCol
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right; width: units.gu(1); height: units.dp(2)
                    color: crosshair.retCol
                }
            }

            // === MULTI-TOUCH DUAL-THUMB + ACTION CLUSTER CONTROLLER ===
            Item {
                id: touchLayer
                anchors.fill: parent
                visible: !rootWin.showTitleScreen && !game.paused && !game.isGameOver && !game.isVictory

                // Visual state for left floating stick
                property bool stickActive: false
                property real stickStartX: units.gu(11)
                property real stickStartY: height - units.gu(11)
                property real stickCurX: stickStartX
                property real stickCurY: stickStartY
                readonly property real stickMaxR: units.gu(5.8)

                // Visual highlight states for action buttons
                property bool fireDown: false

                function inCircle(px, py, item) {
                    var c = item.mapToItem(touchLayer, item.width * 0.5, item.height * 0.5)
                    var dx = px - c.x
                    var dy = py - c.y
                    var r = item.width * 0.58
                    return (dx * dx + dy * dy) <= (r * r)
                }

                // Left Floating Virtual Joystick Graphic
                Rectangle {
                    visible: rootWin.showTouchControls
                    x: (touchLayer.stickActive ? touchLayer.stickStartX : units.gu(10.5)) - width * 0.5
                    y: (touchLayer.stickActive ? touchLayer.stickStartY : (parent.height - units.gu(10.5))) - height * 0.5
                    width: touchLayer.stickMaxR * 2
                    height: width
                    radius: width * 0.5
                    color: "#220a1828"
                    border.width: units.dp(2)
                    border.color: touchLayer.stickActive ? "#aa28f7ff" : "#5528f7ff"

                    Rectangle {
                        width: units.gu(4.6)
                        height: width
                        radius: width * 0.5
                        x: (parent.width - width) * 0.5 + (touchLayer.stickActive ? (touchLayer.stickCurX - touchLayer.stickStartX) : 0)
                        y: (parent.height - height) * 0.5 + (touchLayer.stickActive ? (touchLayer.stickCurY - touchLayer.stickStartY) : 0)
                        color: touchLayer.stickActive ? "#cc28f7ff" : "#5528f7ff"
                        border.width: units.dp(2)
                        border.color: "#ffffff"
                    }
                }

                // Right-Side Ergonomic Boomer-Shooter Action Cluster
                // 1) Primary FIRE Trigger (supports hold-to-fire AND simultaneous drag-to-aim!)
                Rectangle {
                    id: btnFire
                    visible: rootWin.showTouchControls
                    width: units.gu(9.5)
                    height: width
                    radius: width * 0.5
                    anchors.right: parent.right
                    anchors.rightMargin: units.gu(3.2)
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: units.gu(3.2)
                    color: touchLayer.fireDown ? "#ddff2bd6" : "#66ff2bd6"
                    border.width: units.dp(2.5)
                    border.color: "#ff88ea"

                    Label {
                        anchors.centerIn: parent
                        text: "FIRE"
                        font.bold: true
                        fontSize: "medium"
                        color: "#ffffff"
                    }
                }

                // 2) MIGHTY BOOT KICK Button (Duke 3D / CRUEL staple!)
                Rectangle {
                    id: btnBoot
                    visible: rootWin.showTouchControls
                    width: units.gu(6.8)
                    height: width
                    radius: width * 0.5
                    anchors.right: btnFire.left
                    anchors.rightMargin: units.gu(1.8)
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: units.gu(2.4)
                    color: "#77ff9d00"
                    border.width: units.dp(2)
                    border.color: "#ffd055"

                    Label {
                        anchors.centerIn: parent
                        text: "BOOT"
                        font.bold: true
                        fontSize: "small"
                        color: "#ffffff"
                    }
                }

                // 3) FAN-THE-HAMMER Lock-On Burst (Ion Fury special!)
                Rectangle {
                    id: btnFan
                    visible: rootWin.showTouchControls
                    width: units.gu(6.4)
                    height: width
                    radius: width * 0.5
                    anchors.right: btnFire.left
                    anchors.rightMargin: units.gu(1.0)
                    anchors.bottom: btnFire.top
                    anchors.bottomMargin: units.gu(0.6)
                    color: "#669c27ff"
                    border.width: units.dp(2)
                    border.color: "#e288ff"

                    Label {
                        anchors.centerIn: parent
                        text: "FAN"
                        font.bold: true
                        fontSize: "small"
                        color: "#ffffff"
                    }
                }

                // 4) JUMP / DASH Dodge Button
                Rectangle {
                    id: btnDash
                    visible: rootWin.showTouchControls
                    width: units.gu(6.4)
                    height: width
                    radius: width * 0.5
                    anchors.right: parent.right
                    anchors.rightMargin: units.gu(2.4)
                    anchors.bottom: btnFire.top
                    anchors.bottomMargin: units.gu(1.8)
                    color: "#5500d8ff"
                    border.width: units.dp(2)
                    border.color: "#77f3ff"

                    Label {
                        anchors.centerIn: parent
                        text: "DASH"
                        font.bold: true
                        fontSize: "small"
                        color: "#ffffff"
                    }
                }

                // Fullscreen 4-Point Simultaneous Touch Tracker (isolated from hardware mouse!)
                MultiPointTouchArea {
                    id: mpta
                    anchors.fill: parent
                    enabled: rootWin.touchHudMode !== 2
                    mouseEnabled: false
                    minimumTouchPoints: 1
                    maximumTouchPoints: 5

                    property int movePtId: -1
                    property int lookPtId: -1
                    property int firePtId: -1
                    property real prevLookX: 0
                    property real prevLookY: 0
                    property real prevFireX: 0
                    property real prevFireY: 0

                    onPressed: {
                        game.notifyTouchActive()
                        for (var i = 0; i < touchPoints.length; ++i) {
                            var tp = touchPoints[i]
                            // Skip top HUD bar & bottom weapon switcher strip
                            if (tp.y < units.gu(5.8)) continue
                            if (tp.y > touchLayer.height - units.gu(5.0) &&
                                tp.x > touchLayer.width * 0.30 && tp.x < touchLayer.width * 0.66) continue

                            if (touchLayer.inCircle(tp.x, tp.y, btnBoot)) {
                                game.bootKick()
                                continue
                            }
                            if (touchLayer.inCircle(tp.x, tp.y, btnFan)) {
                                game.altFire()
                                continue
                            }
                            if (touchLayer.inCircle(tp.x, tp.y, btnDash)) {
                                game.jumpOrDash()
                                continue
                            }
                            if (touchLayer.inCircle(tp.x, tp.y, btnFire)) {
                                firePtId = tp.pointId
                                prevFireX = tp.x
                                prevFireY = tp.y
                                touchLayer.fireDown = true
                                game.setFireHeld(true)
                                continue
                            }

                            if (tp.x < touchLayer.width * 0.43 && movePtId === -1) {
                                movePtId = tp.pointId
                                touchLayer.stickActive = true
                                touchLayer.stickStartX = tp.x
                                touchLayer.stickStartY = tp.y
                                touchLayer.stickCurX = tp.x
                                touchLayer.stickCurY = tp.y
                                game.setMoveInput(0, 0)
                            } else if (lookPtId === -1) {
                                lookPtId = tp.pointId
                                prevLookX = tp.x
                                prevLookY = tp.y
                            }
                        }
                    }

                    onUpdated: {
                        for (var i = 0; i < touchPoints.length; ++i) {
                            var tp = touchPoints[i]
                            if (tp.pointId === movePtId) {
                                var dx = tp.x - touchLayer.stickStartX
                                var dy = tp.y - touchLayer.stickStartY
                                var dist = Math.sqrt(dx * dx + dy * dy)
                                var maxR = touchLayer.stickMaxR
                                if (dist > maxR) {
                                    dx = dx * maxR / dist
                                    dy = dy * maxR / dist
                                }
                                touchLayer.stickCurX = touchLayer.stickStartX + dx
                                touchLayer.stickCurY = touchLayer.stickStartY + dy
                                game.setMoveInput(-dy / maxR, dx / maxR)
                            } else if (tp.pointId === lookPtId) {
                                game.addTouchLook(tp.x - prevLookX, tp.y - prevLookY)
                                prevLookX = tp.x
                                prevLookY = tp.y
                            } else if (tp.pointId === firePtId) {
                                // Aim smoothly even while dragging thumb across FIRE button!
                                game.addTouchLook(tp.x - prevFireX, tp.y - prevFireY)
                                prevFireX = tp.x
                                prevFireY = tp.y
                            }
                        }
                    }

                    function handleRelease(pts) {
                        for (var i = 0; i < pts.length; ++i) {
                            var tp = pts[i]
                            if (tp.pointId === movePtId) {
                                movePtId = -1
                                touchLayer.stickActive = false
                                game.setMoveInput(0, 0)
                            }
                            if (tp.pointId === lookPtId) {
                                lookPtId = -1
                            }
                            if (tp.pointId === firePtId) {
                                firePtId = -1
                                touchLayer.fireDown = false
                                game.setFireHeld(false)
                            }
                        }
                    }

                    onReleased: handleRelease(touchPoints)
                    onCanceled: handleRelease(touchPoints)
                }

                // Subtle Convergence / Keyboard-Mouse Hint Strip when Touch HUD is hidden
                Rectangle {
                    visible: !rootWin.showTouchControls
                    anchors.left: parent.left
                    anchors.leftMargin: units.gu(1.2)
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: units.gu(1.0)
                    width: kbHintLbl.implicitWidth + units.gu(1.8)
                    height: units.gu(2.8)
                    radius: units.gu(0.6)
                    color: "#770c0518"
                    border.width: units.dp(1)
                    border.color: "#4428f7ff"

                    Label {
                        id: kbHintLbl
                        anchors.centerIn: parent
                        text: "WASD Move · Mouse Aim · LMB Fire · RMB Fan · F/MMB Boot · Space Dash · E Use · Wheel Weapons · Tab Touch HUD · Esc Pause"
                        fontSize: "x-small"
                        color: "#b4e8ff"
                    }
                }

                // Bottom-Center Quick Weapon Bar (1..4)
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: units.gu(0.8)
                    spacing: units.gu(0.7)

                    Repeater {
                        model: [
                            { idx: 0, label: "1 .44", owned: true },
                            { idx: 1, label: "2 SHOT", owned: game.ownShotgun },
                            { idx: 2, label: "3 RIP", owned: game.ownChaingun },
                            { idx: 3, label: "4 RPG", owned: game.ownRocket }
                        ]
                        delegate: Rectangle {
                            width: units.gu(7.0)
                            height: units.gu(3.8)
                            radius: units.gu(0.7)
                            opacity: modelData.owned ? 1.0 : 0.35
                            color: game.currentWeapon === modelData.idx ? "#ccff2bd6" : "#8812091e"
                            border.width: units.dp(1.5)
                            border.color: game.currentWeapon === modelData.idx ? "#ffffff" : "#28f7ff"

                            Label {
                                anchors.centerIn: parent
                                text: modelData.label
                                font.bold: true
                                fontSize: "x-small"
                                color: "#ffffff"
                            }
                            MouseArea {
                                anchors.fill: parent
                                onPressed: if (modelData.owned) game.selectWeapon(modelData.idx)
                            }
                        }
                    }
                }

                // Context-Sensitive Interact Pill (Doors / Exit / Soda / Secret)
                Rectangle {
                    visible: game.contextPrompt.length > 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: units.gu(5.6)
                    width: promptLbl.implicitWidth + units.gu(3)
                    height: units.gu(4.0)
                    radius: units.gu(2.0)
                    color: "#dd18092c"
                    border.width: units.dp(2)
                    border.color: "#28f7ff"

                    Label {
                        id: promptLbl
                        anchors.centerIn: parent
                        text: game.contextPrompt
                        font.bold: true
                        fontSize: "small"
                        color: "#28f7ff"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: game.interact()
                    }
                }
            }

            // === TOP NEON BOOMER-SHOOTER HUD BAR ===
            Rectangle {
                id: topHud
                visible: !rootWin.showTitleScreen
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: units.gu(5.4)
                color: "#bb0c0518"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: units.dp(2)
                    color: "#ff2bd6"
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: units.gu(1.2)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: units.gu(1.4)

                    Label {
                        text: "HP " + game.health
                        font.bold: true
                        fontSize: "medium"
                        color: game.health <= 30 ? "#ff384c" : "#3aff8a"
                    }
                    Label {
                        text: "ARM " + game.armor
                        font.bold: true
                        fontSize: "small"
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#28f7ff"
                    }
                    Label {
                        text: game.currentAmmo >= 900 ? "AMMO INF" : ("AMMO " + game.currentAmmo)
                        font.bold: true
                        fontSize: "small"
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#ffd24a"
                    }
                    Rectangle {
                        visible: game.hasKeycard
                        width: units.gu(4.8)
                        height: units.gu(2.6)
                        radius: units.dp(4)
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#ffbe1e"
                        Label {
                            anchors.centerIn: parent
                            text: "KEY"
                            font.bold: true
                            fontSize: "x-small"
                            color: "#12081c"
                        }
                    }
                }

                // Center CRUEL Adrenaline Blood-Clock + Combo Multiplier!
                Row {
                    anchors.centerIn: parent
                    spacing: units.gu(1.2)

                    Rectangle {
                        visible: game.isTimedMode
                        width: units.gu(9.5)
                        height: units.gu(3.8)
                        radius: units.gu(0.8)
                        color: game.timeLeft < 7.5 ? "#ddff1e42" : "#bb1c0b34"
                        border.width: units.dp(2)
                        border.color: game.timeLeft < 7.5 ? "#ffffff" : "#ff2bd6"

                        Label {
                            anchors.centerIn: parent
                            text: game.timeLeft.toFixed(1) + "s"
                            font.bold: true
                            fontSize: "medium"
                            color: "#ffffff"
                        }
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: game.combo > 1 ? ("x" + game.combo + "  " + game.score) : ("PTS " + game.score)
                        font.bold: true
                        fontSize: "small"
                        color: game.combo >= 5 ? "#ff2bd6" : "#ffffff"
                    }
                }

                // Right: Floor Info, 90° Rotate Button & Lomiri Pause Menu Trigger
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: units.gu(1.0)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: units.gu(0.8)

                    Label {
                        text: "ENEMIES " + game.enemiesAlive
                        fontSize: "x-small"
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#b8c6db"
                    }

                    Rectangle {
                        width: units.gu(4.4)
                        height: units.gu(3.6)
                        radius: units.gu(0.6)
                        color: "#3328f7ff"
                        border.width: units.dp(1)
                        border.color: "#28f7ff"
                        Label {
                            anchors.centerIn: parent
                            text: "90°"
                            font.bold: true
                            fontSize: "x-small"
                            color: "#28f7ff"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: rootWin.forceLandscape = !rootWin.forceLandscape
                        }
                    }

                    Rectangle {
                        width: units.gu(4.6)
                        height: units.gu(3.6)
                        radius: units.gu(0.6)
                        color: "#55ff2bd6"
                        border.width: units.dp(1.5)
                        border.color: "#ff2bd6"
                        Label {
                            anchors.centerIn: parent
                            text: "II"
                            font.bold: true
                            fontSize: "small"
                            color: "#ffffff"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: game.paused = !game.paused
                        }
                    }
                }
            }

            // Boss Health Bar (Floor 4 Warlord Heskel-X!)
            Rectangle {
                visible: !rootWin.showTitleScreen && game.bossName.length > 0 && game.bossHealthRatio > 0
                anchors.top: topHud.bottom
                anchors.topMargin: units.gu(0.8)
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width * 0.62, units.gu(42))
                height: units.gu(2.4)
                radius: units.dp(5)
                color: "#cc1a0718"
                border.width: units.dp(1.5)
                border.color: "#ff384c"

                Rectangle {
                    x: units.dp(2); y: units.dp(2)
                    width: (parent.width - units.dp(4)) * game.bossHealthRatio
                    height: parent.height - units.dp(4)
                    radius: units.dp(4)
                    color: "#ff2442"
                }
                Label {
                    anchors.centerIn: parent
                    text: game.bossName
                    font.bold: true
                    fontSize: "x-small"
                    color: "#ffffff"
                }
            }

            // Badass Boomer-Shooter One-Liner Callout Banner!
            Label {
                visible: !rootWin.showTitleScreen && game.calloutText.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                y: topHud.height + units.gu(3.8)
                text: game.calloutText
                font.bold: true
                fontSize: "large"
                color: "#ffea38"
                style: Text.Outline
                styleColor: "#24052e"
            }

            // === UBUNTU TOUCH / LOMIRI TITLE, PAUSE & SETTINGS OVERLAY ===
            Rectangle {
                id: menuOverlay
                visible: rootWin.showTitleScreen || game.paused || game.isGameOver || game.isVictory
                anchors.fill: parent
                color: rootWin.showTitleScreen ? "#dd0c0418" : "#cc0c0418"

                Flickable {
                    anchors.fill: parent
                    anchors.margins: units.gu(2)
                    contentWidth: width
                    contentHeight: menuCol.implicitHeight + units.gu(3)
                    clip: true

                    Column {
                        id: menuCol
                        width: Math.min(parent.width, units.gu(54))
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: units.gu(1.3)

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: game.isVictory ? "VICTORY — HAIL TO THE QUEEN!" :
                                  game.isGameOver ? "WASTED // FLATLINED" :
                                  rootWin.showTitleScreen ? "CRUEL FURY 3D" : "PAUSED // " + game.floorTitle
                            font.bold: true
                            fontSize: "x-large"
                            color: game.isVictory ? "#3aff8a" : (game.isGameOver ? "#ff3855" : "#ff2bd6")
                        }

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (game.isGameOver || game.isVictory)
                                  ? game.endSummary
                                  : ("HIGH SCORE: " + game.highScore + "  ·  DUKE 3D × CRUEL × ION FURY")
                            horizontalAlignment: Text.AlignHCenter
                            fontSize: "small"
                            color: "#28f7ff"
                        }

                        // Primary Launch / Resume / Retry Lomiri Buttons
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: units.gu(1.5)

                            Button {
                                visible: !rootWin.showTitleScreen && !game.isGameOver && !game.isVictory
                                text: "RESUME"
                                color: LomiriColors.green
                                width: units.gu(15)
                                onClicked: {
                                    game.paused = false
                                    stage.forceActiveFocus()
                                }
                            }

                            Button {
                                text: rootWin.showTitleScreen ? "PLAY CRUEL RUSH" : "NEW RUN"
                                color: "#e91e9c"
                                width: units.gu(19)
                                onClicked: {
                                    rootWin.showTitleScreen = false
                                    game.startGame(game.gameMode, game.difficulty)
                                    stage.forceActiveFocus()
                                }
                            }
                        }

                        // Mode Selector Row (CRUEL Timed / DUKE Classic / ENDLESS Arena)
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: units.gu(1.0)

                            Repeater {
                                model: [
                                    { m: 0, title: "CRUEL CLOCK" },
                                    { m: 1, title: "DUKE CLASSIC" },
                                    { m: 2, title: "ENDLESS WAVE" }
                                ]
                                delegate: Button {
                                    text: modelData.title
                                    width: units.gu(15.5)
                                    color: game.gameMode === modelData.m ? "#0099cc" : "#2c223e"
                                    onClicked: game.gameMode = modelData.m
                                }
                            }
                        }

                        // Difficulty Selector Row
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: units.gu(1.0)

                            Repeater {
                                model: [
                                    { d: 0, title: "CHILL" },
                                    { d: 1, title: "LET'S ROCK" },
                                    { d: 2, title: "ULTRA CRUEL" }
                                ]
                                delegate: Button {
                                    text: modelData.title
                                    width: units.gu(15.5)
                                    color: game.difficulty === modelData.d ? "#ff7b00" : "#2c223e"
                                    onClicked: game.difficulty = modelData.d
                                }
                            }
                        }

                        // Lomiri Touch & Hardware Settings Card
                        Rectangle {
                            width: parent.width
                            height: settingsInner.implicitHeight + units.gu(2.4)
                            radius: units.gu(1.2)
                            color: "#991a0d2e"
                            border.width: units.dp(1)
                            border.color: "#5528f7ff"

                            Column {
                                id: settingsInner
                                anchors.fill: parent
                                anchors.margins: units.gu(1.2)
                                spacing: units.gu(0.9)

                                Row {
                                    width: parent.width
                                    spacing: units.gu(1.2)
                                    Label {
                                        text: "Look Speed: " + game.lookSensitivity.toFixed(1) + "x"
                                        width: units.gu(15)
                                        anchors.verticalCenter: parent.verticalCenter
                                        fontSize: "small"
                                        color: "#ffffff"
                                    }
                                    Slider {
                                        width: parent.width - units.gu(17)
                                        minimumValue: 0.4
                                        maximumValue: 2.4
                                        live: true
                                        Component.onCompleted: value = game.lookSensitivity
                                        onValueChanged: {
                                            if (Math.abs(game.lookSensitivity - value) > 0.01) {
                                                game.lookSensitivity = value
                                            }
                                        }
                                    }
                                }

                                Grid {
                                    columns: 3
                                    spacing: units.gu(1.0)
                                    anchors.horizontalCenter: parent.horizontalCenter

                                    Button {
                                        width: units.gu(14.8)
                                        text: rootWin.forceLandscape ? "View: 90° Wide" : "View: Native"
                                        color: rootWin.forceLandscape ? "#1f6f8b" : "#2c223e"
                                        onClicked: rootWin.forceLandscape = !rootWin.forceLandscape
                                    }
                                    Button {
                                        width: units.gu(14.8)
                                        text: game.gyroEnabled ? "Gyro Aim: ON" : "Gyro Aim: OFF"
                                        color: game.gyroEnabled ? "#1f6f8b" : "#2c223e"
                                        onClicked: game.gyroEnabled = !game.gyroEnabled
                                    }
                                    Button {
                                        width: units.gu(14.8)
                                        text: game.autoFire ? "AutoFire: ON" : "AutoFire: OFF"
                                        color: game.autoFire ? "#1f6f8b" : "#2c223e"
                                        onClicked: game.autoFire = !game.autoFire
                                    }
                                    Button {
                                        width: units.gu(14.8)
                                        text: game.musicEnabled ? "Synth BGM: ON" : "Synth BGM: OFF"
                                        color: game.musicEnabled ? "#6a1b9a" : "#2c223e"
                                        onClicked: game.musicEnabled = !game.musicEnabled
                                    }
                                    Button {
                                        width: units.gu(14.8)
                                        text: game.sfxEnabled ? "SFX: ON" : "SFX: OFF"
                                        color: game.sfxEnabled ? "#6a1b9a" : "#2c223e"
                                        onClicked: game.sfxEnabled = !game.sfxEnabled
                                    }
                                    Button {
                                        width: units.gu(14.8)
                                        text: game.hapticsEnabled ? "Haptics: ON" : "Haptics: OFF"
                                        color: game.hapticsEnabled ? "#6a1b9a" : "#2c223e"
                                        onClicked: game.hapticsEnabled = !game.hapticsEnabled
                                    }
                                    Button {
                                        width: units.gu(14.8)
                                        text: rootWin.touchHudMode === 0 ? "Touch HUD: Auto"
                                            : (rootWin.touchHudMode === 1 ? "Touch HUD: ON" : "Touch HUD: OFF")
                                        color: rootWin.touchHudMode === 2 ? "#2c223e" : "#1f6f8b"
                                        onClicked: rootWin.touchHudMode = (rootWin.touchHudMode + 1) % 3
                                    }
                                }

                                Label {
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                    fontSize: "x-small"
                                    color: "#b4c5e4"
                                    text: "TOUCH: Left thumb moves · Right thumb aims · Drag FIRE to aim!   MOUSE+KB: WASD/Arrows Move · Hover Mouse Aim · LMB Fire · RMB Fan · F/MMB Boot · Space Dash · E Use · 1-4/Wheel Weapons"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
