import QtQuick
import QtQuick.Layouts
import QtQuick.Controls


Rectangle {
    id: root
    objectName: "oscilloscope"
    minWidth: 880
    minHeight: 400

    signal signalSumModeChecked(bool checked);

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        FontLoader {
            id: nekstFont
            source: "qrc:/fonts/Nekst-Light-Desktop.otf"
        }

        Rectangle {
            Layout.fillWidth: true
            color: "white"
            height: 5
        }

        ControlPanelTop {
            id: controlPanelTop
            height: 32
            Layout.fillWidth: true
            labelsFontName: nekstFont.name

            signal passSetIn0ButtonChecked
            signal passSetIn1ButtonChecked
            signal passZoomRectToolPicked(bool picked)
            signal passVertZoomToolPicked(bool picked);
            signal passHorZoomToolPicked(bool picked);
            signal passMoveWorkspaceToolPicked(bool picked)
            signal setIn0ButtonVisible(bool visible)
            signal setIn1ButtonVisible(bool visible)
            onResetZoom: oscView.resetZoom()
            onSignalInput0Checked: checked => oscView.signalPassInput0Checked(checked);
            onSignalInput1Checked: checked => oscView.signalPassInput1Checked(checked);
            onZoomRectToolPicked: picked => passZoomRectToolPicked(picked);
            onVertZoomToolPicked: picked => passVertZoomToolPicked(picked);
            onHorZoomToolPicked: picked => passHorZoomToolPicked(picked);
            onMoveWorkspaceToolPicked: picked => passMoveWorkspaceToolPicked(picked);
            onSignalSumModeChecked: checked => root.signalSumModeChecked(checked);
        }

        Rectangle {
            color: "white"
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 200
            Layout.minimumWidth: 300

            OscView {
                id: oscView
                anchors.fill: parent

                signal resetZoom()
                signal signalPassInput0Checked(bool checked)
                signal signalPassInput1Checked(bool checked)
                signal uncheckRectZoom()
                signal uncheckVertZoom()
                signal uncheckHorZoom()
                signal uncheckMoveWorkspace()
                signal zoomSelected(real x_1, real y_1, real x_2, real y_2)
                signal xZoomSelected(real x_1, real x_2)
                signal yZoomSelected(real y_1, real y_2)
                signal moveWorkspace(real x_1, real y_1, real x_2, real y_2)

                onSetIn0ButtonChecked: controlPanelTop.passSetIn0ButtonChecked()
                onSetIn1ButtonChecked: controlPanelTop.passSetIn1ButtonChecked()
                onSetIn0ButtonVisible: visible => controlPanelTop.setIn0ButtonVisible(visible)
                onSetIn1ButtonVisible: visible => controlPanelTop.setIn1ButtonVisible(visible)

                MouseArea {
                    id: chartMouseArea
                    property real x_pr: 0.0
                    property real x_rel: 0.0
                    property real y_pr: 0.0
                    property real y_rel: 0.0
                    property bool rectZoomPressed: false
                    property bool vertZoomPressed: false
                    property bool horZoomPressed: false
                    property bool firstBorderPlaced: false
                    property bool moveWorkspacePressed: false
                    property bool shiftPressed: false
                    property bool ctrlPressed: false
                    acceptedButtons: Qt.LeftButton
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true

                    signal cursorX(string x)
                    signal cursorY(string y)

                    onEntered: {
                        if (chartMouseArea.vertZoomPressed || chartMouseArea.horZoomPressed) {
                            zoomBorder1.visible = true
                            chartMouseArea.focus = true
                        }
                    }

                    onPositionChanged: mouse => {
                        var cc = oscView.chartCoordinates(mouse.x, mouse.y)
                        if (cc) {
                            cursorX(cc.x.toLocaleString(Qt.locale("ru_RU"), "%g", 0))
                            cursorY(cc.y.toLocaleString(Qt.locale("ru_RU"), "%g", 0))
                        }
                        if (chartMouseArea.rectZoomPressed) {
                            zoomRect.width = Math.abs(mouse.x - chartMouseArea.x_pr);
                            if (mouse.x < chartMouseArea.x_pr)
                                zoomRect.x = mouse.x;
                            zoomRect.height = Math.abs(mouse.y - chartMouseArea.y_pr);
                            if (mouse.y < chartMouseArea.y_pr)
                                zoomRect.y = mouse.y;
                        }
                        if (chartMouseArea.moveWorkspacePressed && mouse.buttons === Qt.LeftButton) {
                            var a = mouse.modifiers & Qt.ControlModifier ? chartMouseArea.x_pr : mouse.x
                            var b = mouse.modifiers & Qt.ShiftModifier ? chartMouseArea.y_pr : mouse.y
                            oscView.moveWorkspace(chartMouseArea.x_pr, chartMouseArea.y_pr, a, b);
                            chartMouseArea.x_pr = mouse.x
                            chartMouseArea.y_pr = mouse.y
                        }
                        if (chartMouseArea.vertZoomPressed) {
                            if (firstBorderPlaced) {
                                zoomBorder2.width = 1
                                zoomBorder2.x = mouse.x
                                zoomBorder2.y = 0
                                zoomBorder2.height = oscView.height
                                zoomBorder2.visible = true
                            }
                            else {
                                zoomBorder1.width = 1
                                zoomBorder1.x = mouse.x
                                zoomBorder1.y = 0
                                zoomBorder1.height = oscView.height
                                zoomBorder1.visible = true
                            }
                        }
                        if (chartMouseArea.horZoomPressed) {
                            if (firstBorderPlaced) {
                                zoomBorder2.height = 1
                                zoomBorder2.y = mouse.y
                                zoomBorder2.x = 0
                                zoomBorder2.width = oscView.width
                                zoomBorder2.visible = true
                            }
                            else {
                                zoomBorder1.y = mouse.y
                                zoomBorder1.height = 1
                                zoomBorder1.x = 0
                                zoomBorder1.width = oscView.width
                                zoomBorder1.visible = true
                            }
                        }
                    }
                    onPressed: mouse => {
                        root.bringToFront()
                        if (chartMouseArea.moveWorkspacePressed) {
                            chartMouseArea.x_pr = mouse.x
                            chartMouseArea.y_pr = mouse.y
                            chartMouseArea.cursorShape = Qt.DragMoveCursor
                        }
                        if (chartMouseArea.rectZoomPressed) {
                            chartMouseArea.x_pr = mouse.x;
                            chartMouseArea.y_pr = mouse.y;
                            zoomRect.x = chartMouseArea.x_pr;
                            zoomRect.y = chartMouseArea.y_pr;
                            zoomRect.visible = true;
                        }
                        if (chartMouseArea.vertZoomPressed && !firstBorderPlaced) {
                            chartMouseArea.x_pr = mouse.x
                            zoomBorder1.width = 1
                            zoomBorder1.x = chartMouseArea.x_pr
                            zoomBorder1.y = 0
                            zoomBorder1.height = oscView.height
                            zoomBorder1.visible = true
                        }
                        if (chartMouseArea.horZoomPressed && !firstBorderPlaced) {
                            chartMouseArea.y_pr = mouse.y
                            zoomBorder1.height = 1
                            zoomBorder1.y = chartMouseArea.y_pr
                            zoomBorder1.x = 0
                            zoomBorder1.width = oscView.width
                            zoomBorder1.visible = true
                        }
                    }
                    onReleased: mouse => {
                        if (chartMouseArea.moveWorkspacePressed) {
                            chartMouseArea.cursorShape = Qt.OpenHandCursor
                        }
                        if (chartMouseArea.rectZoomPressed) {
                            chartMouseArea.x_rel = mouse.x;
                            chartMouseArea.y_rel = mouse.y;
                            oscView.zoomSelected(chartMouseArea.x_pr, chartMouseArea.y_pr, chartMouseArea.x_rel, chartMouseArea.y_rel);
                            chartMouseArea.rectZoomPressed = false;
                            chartMouseArea.cursorShape = Qt.ArrowCursor;
                            zoomRect.visible = false;
                            oscView.uncheckRectZoom();
                        }
                        if (chartMouseArea.vertZoomPressed) {
                            if (firstBorderPlaced) {
                                chartMouseArea.x_rel = mouse.x
                                zoomBorder2.width = 1
                                zoomBorder2.x = mouse.x
                                zoomBorder2.y = 0
                                zoomBorder2.height = oscView.height
                                oscView.xZoomSelected(chartMouseArea.x_pr, chartMouseArea.x_rel);
                                zoomBorder1.visible = false
                                zoomBorder2.visible = false
                                firstBorderPlaced = false
                                chartMouseArea.cursorShape = Qt.ArrowCursor;
                                chartMouseArea.vertZoomPressed = false
                                // chartMouseArea.hoverEnabled = false
                                oscView.uncheckVertZoom();
                            } else {
                                firstBorderPlaced = true
                            }
                        }
                        if (chartMouseArea.horZoomPressed) {
                            if (firstBorderPlaced) {
                                chartMouseArea.y_rel = mouse.y
                                zoomBorder2.height = 1
                                zoomBorder2.y = mouse.y
                                zoomBorder2.x = 0
                                zoomBorder2.width = oscView.width
                                oscView.yZoomSelected(chartMouseArea.y_pr, chartMouseArea.y_rel);
                                zoomBorder1.visible = false
                                zoomBorder2.visible = false
                                firstBorderPlaced = false
                                chartMouseArea.horZoomPressed = false
                                // chartMouseArea.hoverEnabled = false
                                chartMouseArea.cursorShape = Qt.ArrowCursor;
                                oscView.uncheckHorZoom();
                            } else {
                                firstBorderPlaced = true
                            }
                        }
                    }

                    Rectangle {
                        id: zoomRect
                        border.color: "black"
                        border.width: 1
                        opacity: 0.1
                        visible: false
                    }
                    Rectangle {
                        id: zoomBorder1
                        border.color: "black"
                        border.width: 1
                        opacity: 0.15
                        visible: false
                    }
                    Rectangle {
                        id: zoomBorder2
                        border.color: "black"
                        border.width: 1
                        opacity: 0.15
                        visible: false
                    }
                    Connections {
                        target: controlPanelTop
                        function onPassZoomRectToolPicked(picked) {
                            if (!picked) {
                                chartMouseArea.cursorShape = Qt.ArrowCursor;
                                chartMouseArea.rectZoomPressed = false;
                                oscView.uncheckRectZoom()
                            } else {
                                chartMouseArea.cursorShape = Qt.CrossCursor;
                                chartMouseArea.rectZoomPressed = true;
                                chartMouseArea.horZoomPressed = false;
                                chartMouseArea.vertZoomPressed = false;
                                chartMouseArea.moveWorkspacePressed = false;
                                oscView.uncheckMoveWorkspace();
                            }
                        }
                        function onPassMoveWorkspaceToolPicked(picked) {
                            if (!picked) {
                                chartMouseArea.cursorShape = Qt.ArrowCursor;
                                chartMouseArea.moveWorkspacePressed = false;
                                oscView.uncheckMoveWorkspace();
                            } else {
                                chartMouseArea.cursorShape = Qt.OpenHandCursor;
                                chartMouseArea.rectZoomPressed = false;
                                chartMouseArea.horZoomPressed = false;
                                chartMouseArea.vertZoomPressed = false;
                                chartMouseArea.moveWorkspacePressed = true;
                                // chartMouseArea.hoverEnabled = false
                                oscView.uncheckRectZoom()
                            }
                        }
                        function onPassVertZoomToolPicked(picked) {
                            if (!picked) {
                                chartMouseArea.cursorShape = Qt.ArrowCursor;
                                zoomBorder1.visible = false
                                zoomBorder2.visible = false
                                chartMouseArea.vertZoomPressed = false;
                                // chartMouseArea.hoverEnabled = false
                                oscView.uncheckVertZoom();
                            } else {
                                chartMouseArea.cursorShape = Qt.CrossCursor;
                                chartMouseArea.moveWorkspacePressed = false;
                                chartMouseArea.rectZoomPressed = false;
                                chartMouseArea.horZoomPressed = false;
                                chartMouseArea.vertZoomPressed = true;
                                zoomBorder1.visible = false
                                zoomBorder2.visible = false
                                chartMouseArea.firstBorderPlaced = false
                                // chartMouseArea.hoverEnabled = true
                                oscView.uncheckMoveWorkspace();
                                oscView.uncheckRectZoom();
                                oscView.uncheckHorZoom();
                            }
                        }
                        function onPassHorZoomToolPicked(picked) {
                            if (!picked) {
                                chartMouseArea.cursorShape = Qt.ArrowCursor;
                                zoomBorder1.visible = false
                                zoomBorder2.visible = false
                                chartMouseArea.horZoomPressed = false;
                                // chartMouseArea.hoverEnabled = false
                                oscView.uncheckHorZoom();
                            } else {
                                chartMouseArea.cursorShape = Qt.CrossCursor;
                                chartMouseArea.moveWorkspacePressed = false;
                                chartMouseArea.rectZoomPressed = false;
                                chartMouseArea.vertZoomPressed = false;
                                chartMouseArea.horZoomPressed = true;
                                zoomBorder1.visible = false
                                zoomBorder2.visible = false
                                // chartMouseArea.hoverEnabled = true
                                chartMouseArea.firstBorderPlaced = false
                                oscView.uncheckMoveWorkspace();
                                oscView.uncheckRectZoom();
                                oscView.uncheckVertZoom();
                            }
                        }
                    }
                    Connections {
                        target: controlPanelBottom
                        function onMoveLeft(x) {
                            var min = oscView.getXMin()
                            var max = oscView.getXMax()
                            oscView.setXMin(min - x);
                            oscView.setXMax(max - x);
                        }
                        function onMoveRight(x) {
                            var min = oscView.getXMin()
                            var max = oscView.getXMax()
                            oscView.setXMin(min + x);
                            oscView.setXMax(max + x);
                        }
                    }
                }
            }

        }


        ControlPanelBottom {
            id: controlPanelBottom
            height: 28
            oscView: oscView
            Layout.fillWidth: true
            labelsFontName: nekstFont.name
            naturalUnits: controlPanelTop.naturalUnits

            Connections {
                target: oscView

                function onZoomIsSet(x_1, y_1, x_2, y_2) {
                    controlPanelBottom.setSbShow(Math.abs(x_2 - x_1))
                    controlPanelBottom.setSbStart(Math.min(x_1, x_2))
                }
                function onXZoomIsSet(x_1, x_2) {
                    controlPanelBottom.setSbShow(Math.abs(x_2 - x_1))
                    controlPanelBottom.setSbStart(Math.min(x_1, x_2))
                }
                function onWSMoved(x_1) {
                    controlPanelBottom.setSbStart(x_1)
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: "white"
            height: 5
        }
    }
}
