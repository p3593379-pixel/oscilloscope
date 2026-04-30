import QtQuick
import QtCharts

ChartView {
    id: reflectogramCV

    property bool openGL: openGLSupported
    property int fps: 25
    property bool in0: in0Acquired
    property bool in1: in1Acquired
    property real frame_size: 8192
    property real dynamic_range: 32768

    plotAreaColor: "transparent"
    animationOptions: ChartView.SeriesAnimations
    theme: ChartView.ChartThemeLight
    backgroundColor: "transparent"
    dropShadowEnabled: false
    // Minimize margins to maximize chart space
    margins.top: 0
    margins.bottom: 0
    margins.left: 0
    margins.right: 0
    backgroundRoundness: 0

    signal setIn0ButtonChecked();
    signal setIn1ButtonChecked();
    signal setIn0ButtonVisible(bool visible);
    signal setIn1ButtonVisible(bool visible);
    signal moveWorkspaceToolPicked(bool picked);
    signal zoomIsSet(real x_1, real y_1, real x_2, real y_2);
    signal xZoomIsSet(real x_1, real x_2);
    signal yZoomIsSet(real y_1, real y_2);
    signal wSMoved(real x_1);

    function chartCoordinates(x, y) {
        if (reflectogramCV.count) {
            var p = Qt.point(x, y);
            return reflectogramCV.mapToValue(p, reflectogramCV.series(0));
        }
    }

    function getXMin() {
        // console.log(Math.round(refAxisX.min))
        return Math.round(refAxisX.min)
    }
    function getXMax() { return Math.round(refAxisX.max) }
    function setXMin(x) { refAxisX.min = x }
    function setXMax(x) { refAxisX.max = x }
    function getYMin() { return Math.round(refAxisY0.min) }
    function getYMax() { return Math.round(refAxisY0.max) }
    function setYMin(y) { refAxisY0.min = y }
    function setYMax(y) { refAxisY0.max = y }


    Connections {
        target: oscView
        function onSignalPassInput0Checked(checked) {
            if (checked) {
                var lineSeries0 = createSeries(ChartView.SeriesTypeLine, "IN0", refAxisX, refAxisY0);
                lineSeries0.useOpenGL = reflectogramCV.openGL
                lineSeries0.color = "#87cefa"
                reflectogramCV.in0 = true
            }
            else {
                reflectogramCV.in0 = false
                reflectogramCV.removeSeries(reflectogramCV.series("IN0"))
            }
        }
        function onSignalPassInput1Checked(checked) {
            if (checked) {
                var lineSeries1 = createSeries(ChartView.SeriesTypeLine, "IN1", refAxisX, refAxisY0);
                lineSeries1.useOpenGL = reflectogramCV.openGL
                lineSeries1.color = "#f08080"
                reflectogramCV.in1 = true
            }
            else {
                reflectogramCV.in1 = false
                reflectogramCV.removeSeries(reflectogramCV.series("IN1"))
            }
        }
        function onZoomSelected(x_1, y_1, x_2, y_2) {
            var p1 = Qt.point(x_1, y_1)
            var p2 = Qt.point(x_2, y_2)
            if (reflectogramCV.count > 0) {
                var p_chart_1 = reflectogramCV.mapToValue(p1, reflectogramCV.series(0))
                var p_chart_2 = reflectogramCV.mapToValue(p2, reflectogramCV.series(0))
                refAxisX.min = p_chart_1.x < p_chart_2.x ? p_chart_1.x : p_chart_2.x;
                refAxisX.max = p_chart_1.x > p_chart_2.x ? p_chart_1.x : p_chart_2.x;
                refAxisY0.min = p_chart_1.y < p_chart_2.y ? p_chart_1.y : p_chart_2.y;
                refAxisY0.max = p_chart_1.y > p_chart_2.y ? p_chart_1.y : p_chart_2.y;
                zoomIsSet(getXMin(), getYMin(), getXMax(), getYMax())
            }
        }
        function onXZoomSelected(x_1, x_2) {
            var p1 = Qt.point(x_1, 0)
            var p2 = Qt.point(x_2, 0)
            if (reflectogramCV.count > 0) {
                var p_chart_1 = reflectogramCV.mapToValue(p1, reflectogramCV.series(0))
                var p_chart_2 = reflectogramCV.mapToValue(p2, reflectogramCV.series(0))
                refAxisX.min = p_chart_1.x < p_chart_2.x ? p_chart_1.x : p_chart_2.x;
                refAxisX.max = p_chart_1.x > p_chart_2.x ? p_chart_1.x : p_chart_2.x;
                xZoomIsSet(getXMin(), getXMax())
            }
        }
        function onYZoomSelected(y_1, y_2) {
            var p1 = Qt.point(0, y_1)
            var p2 = Qt.point(0, y_2)
            if (reflectogramCV.count > 0) {
                var p_chart_1 = reflectogramCV.mapToValue(p1, reflectogramCV.series(0))
                var p_chart_2 = reflectogramCV.mapToValue(p2, reflectogramCV.series(0))
                refAxisY0.min = p_chart_1.y < p_chart_2.y ? p_chart_1.y : p_chart_2.y;
                refAxisY0.max = p_chart_1.y > p_chart_2.y ? p_chart_1.y : p_chart_2.y;
                yZoomIsSet(getYMin(), getYMax())
            }
        }
        function onResetZoom() {
            refAxisX.min = 0
            refAxisX.max = reflectogramCV.frame_size
            refAxisY0.min = - reflectogramCV.dynamic_range
            refAxisY0.max = reflectogramCV.dynamic_range
            zoomIsSet(getXMin(), getYMin(), getXMax(), getYMax())
        }
        function onMoveWorkspace(x_1, y_1, x_2, y_2) {
            var p_1 = Qt.point(x_1, y_1)
            var p_2 = Qt.point(x_2, y_2)
            if (reflectogramCV.count > 0) {
                var p_chart_1 = reflectogramCV.mapToValue(p_1, reflectogramCV.series(0))
                var p_chart_2 = reflectogramCV.mapToValue(p_2, reflectogramCV.series(0))
                refAxisX.min -= (p_chart_2.x - p_chart_1.x)
                refAxisX.max -= (p_chart_2.x - p_chart_1.x)
                refAxisY0.min -= (p_chart_2.y - p_chart_1.y)
                refAxisY0.max -= (p_chart_2.y - p_chart_1.y)
                wSMoved(getXMin())
            }
        }
    }

    Connections {
        target: RefOsc
        function onXLimChanged(min, max) {
            reflectogramCV.frame_size = max
            refAxisX.min = min;
            refAxisX.max = max;
        }
        function onNewFrame() {
            if (reflectogramCV.in0)
                RefOsc.updateSignal(reflectogramCV.series("IN0"), 0);
            if (reflectogramCV.in1)
                RefOsc.updateSignal(reflectogramCV.series("IN1"), 1);
        }
        function onChannelsChanged(_in0, _in1) {
            reflectogramCV.in0 = _in0;
            reflectogramCV.in1 = _in1;
            removeAllSeries();
            setIn0ButtonVisible(_in0);
            setIn1ButtonVisible(_in1);
            if (in0) {
                var lineSeries0 = createSeries(ChartView.SeriesTypeLine, "IN0", refAxisX, refAxisY0);
                lineSeries0.useOpenGL = reflectogramCV.openGL
                lineSeries0.color = "#87cefa"
                setIn0ButtonChecked()
            }
            if (in1) {
                var lineSeries1 = createSeries(ChartView.SeriesTypeLine, "IN1", refAxisX, refAxisY0);
                lineSeries1.useOpenGL = reflectogramCV.openGL
                lineSeries1.color = "#f08080"
                setIn1ButtonChecked()
            }
        }
    }

    ValueAxis {
        id: refAxisY0
        min: - dynamic_range
        max: dynamic_range
        tickType: ValueAxis.TicksDynamic
        tickAnchor: 0
        minorTickCount: 10
        tickInterval: 10000
        labelFormat: "%d"
    }

    ValueAxis {
        id: refAxisX
        min: 0
        max: frame_size
        tickType: ValueAxis.TicksDynamic
        tickAnchor: 0
        minorTickCount: 5
        tickInterval: 2000
        labelFormat: "%d"
    }

    LineSeries {
        id: lineSeries0
        name: "IN0"
        axisX: refAxisX
        axisY: refAxisY0
        useOpenGL: reflectogramCV.openGL
        color: "#87cefa"
    }
    LineSeries {
        id: lineSeries1
        name: "IN1"
        axisX: refAxisX
        axisYRight: refAxisY0
        useOpenGL: reflectogramCV.openGL
        color: "#f08080"
    }
}
