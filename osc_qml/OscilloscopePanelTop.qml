import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Elements.ToolTip

Rectangle {
    id: root
    height: 26
    color: "white"
    border.width: 0
    property string labelsFontName: "monospace"
    readonly property int fontSize: Math.max(8, Math.round(root.height * 12 / 28))
    property bool naturalUnits: false

    signal signalInput0Checked(bool checked);
    signal signalInput1Checked(bool checked);
    signal zoomRectToolPicked(bool picked);
    signal vertZoomToolPicked(bool picked);
    signal horZoomToolPicked(bool picked);
    signal resetZoom();
    signal moveWorkspaceToolPicked(bool picked);
    signal signalSumModeChecked(bool checked);

    Connections {
        target: controlPanelTop
        function onPassSetIn0ButtonChecked() {
            in0Button.visible = true
            in0Button.checked = true
        }
        function onPassSetIn1ButtonChecked() {
            in1Button.visible = true
            in1Button.checked = true
        }
        function onSetIn0ButtonVisible(visible) {
            in0Button.visible = visible
        }
        function onSetIn1ButtonVisible(visible) {
            in1Button.visible = visible
        }
    }

    Connections {
        target: oscView
        function onUncheckRectZoom() {
            rectZoomToolButton.checked = false
        }
        function onUncheckMoveWorkspace() {
            moveWorkspaceToolButton.checked = false
        }
        function onUncheckVertZoom() {
            vertZoomToolButton.checked = false
        }
        function onUncheckHorZoom() {
            horZoomToolButton.checked = false
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        // Channel Input Buttons
        Button {
            id: in0Button
            Layout.preferredHeight: root.height - 4
            Layout.preferredWidth: root.height - 2
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: true
            visible: in0Acquired
            text: "IN0"
            font.pixelSize: root.fontSize - 2
            font.bold: false
            font.family: root.labelsFontName

            background: Rectangle {
                color: in0Button.checked ? "#87cefa" : "#e0e0e0"
                radius: 3
                border.color: in0Button.checked ? "#5ca9d9" : "#c0c0c0"
                border.width: 1
            }

            onClicked: signalInput0Checked(in0Button.checked)
        }

        Button {
            id: in1Button
            Layout.preferredHeight: root.height - 4
            Layout.preferredWidth: root.height - 2
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: true
            visible: in1Acquired
            text: "IN1"
            font.pixelSize: root.fontSize - 2
            font.bold: false
            font.family: root.labelsFontName

            background: Rectangle {
                color: in1Button.checked ? "#f08080" : "#e0e0e0"
                radius: 3
                border.color: in1Button.checked ? "#d66060" : "#c0c0c0"
                border.width: 1
            }

            onClicked: signalInput1Checked(in1Button.checked)
        }

        // Vertical separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: root.height - 10
            Layout.alignment: Qt.AlignVCenter
            color: "#d0d0d0"
        }

        // Zoom and Pan Tools
        ToolButton {
            id: rectZoomToolButton
            Layout.preferredWidth: root.height - 2
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: false
            icon.source: "qrc:/img/rectZoom.svg"
            icon.width: root.height - 4
            icon.height: root.height - 4
            flat: !checked

            ToolTip {
                visible: parent.hovered
                text: "Rectangle Zoom"
                fontFamily: root.labelsFontName
            }

            onClicked: {
                moveWorkspaceToolButton.checked = false
                horZoomToolButton.checked = false
                vertZoomToolButton.checked = false
                moveWorkspaceToolPicked(false)
                horZoomToolPicked(false)
                vertZoomToolPicked(false)
                zoomRectToolPicked(rectZoomToolButton.checked)
            }
        }

        ToolButton {
            id: vertZoomToolButton
            Layout.preferredWidth: root.height - 2
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: false
            icon.source: "qrc:/img/vertZoom.svg"
            icon.width: root.height - 4
            icon.height: root.height - 4
            flat: !checked

            ToolTip {
                visible: parent.hovered
                text: "Vertical Zoom"
                fontFamily: root.labelsFontName
            }

            onClicked: {
                moveWorkspaceToolButton.checked = false
                rectZoomToolButton.checked = false
                horZoomToolButton.checked = false
                moveWorkspaceToolPicked(false)
                zoomRectToolPicked(false)
                horZoomToolPicked(false)
                vertZoomToolPicked(vertZoomToolButton.checked)
            }
        }

        ToolButton {
            id: horZoomToolButton
            Layout.preferredWidth: root.height - 2
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: false
            icon.source: "qrc:/img/horZoom.svg"
            icon.width: root.height - 4
            icon.height: root.height - 4
            flat: !checked

            ToolTip {
                visible: parent.hovered
                text: "Horizontal Zoom"
                fontFamily: root.labelsFontName
            }

            onClicked: {
                moveWorkspaceToolButton.checked = false
                rectZoomToolButton.checked = false
                vertZoomToolButton.checked = false
                moveWorkspaceToolPicked(false)
                zoomRectToolPicked(false)
                vertZoomToolPicked(false)
                horZoomToolPicked(horZoomToolButton.checked)
            }
        }

        ToolButton {
            id: resetZoomButton
            Layout.preferredWidth: root.height - 2
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            icon.source: "qrc:/img/resetZoom.svg"
            icon.width: root.height - 4
            icon.height: root.height - 4
            flat: true

            ToolTip {
                visible: parent.hovered
                text: "Reset Zoom"
                fontFamily: root.labelsFontName
            }

            onClicked: {
                rectZoomToolButton.checked = false
                moveWorkspaceToolButton.checked = false
                vertZoomToolButton.checked = false
                horZoomToolButton.checked = false
                zoomRectToolPicked(false)
                moveWorkspaceToolPicked(false)
                vertZoomToolPicked(false)
                horZoomToolPicked(false)
                resetZoom()
            }
        }

        ToolButton {
            id: moveWorkspaceToolButton
            Layout.preferredWidth: root.height - 2
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: false
            icon.source: "qrc:/img/moveArrows.svg"
            icon.width: root.height - 4
            icon.height: root.height - 4
            flat: !checked

            ToolTip {
                visible: parent.hovered
                text: "Pan View"
                fontFamily: root.labelsFontName
            }

            onClicked: {
                rectZoomToolButton.checked = false
                vertZoomToolButton.checked = false
                horZoomToolButton.checked = false
                zoomRectToolPicked(false)
                vertZoomToolPicked(false)
                horZoomToolPicked(false)
                moveWorkspaceToolPicked(moveWorkspaceToolButton.checked)
            }
        }

        ToolButton {
            id: summSignalMode
            Layout.preferredWidth: root.height - 2
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            checkable: true
            checked: false
            visible: false
            icon.source: "qrc:/img/sum.svg"
            icon.width: root.height - 4
            icon.height: root.height - 4
            flat: !checked

            ToolTip {
                visible: parent.hovered
                text: "Sum Mode"
                fontFamily: root.labelsFontName
            }
            onClicked: signalSumModeChecked(summSignalMode.checked)
        }

        // Vertical separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: root.height - 10
            Layout.alignment: Qt.AlignVCenter
            color: "#d0d0d0"
        }

        // Units selector
        Label {
            text: "Units:"
            Layout.alignment: Qt.AlignVCenter
            font.family: root.labelsFontName
            font.pixelSize: root.fontSize
            color: "black"
        }

        Rectangle {
            Layout.preferredWidth: 120
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            color: "#e0e0e0"
            radius: 3
            border.color: "#c0c0c0"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Button {
                    id: naturalButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    checkable: true
                    checked: true
                    text: "Natural"
                    font.family: root.labelsFontName
                    font.pixelSize: root.fontSize - 2

                    background: Rectangle {
                        color: naturalButton.checked ? "#c4c4c4" : "transparent"
                        radius: 3
                        Rectangle {
                            width: 2
                            height: parent.height
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#c4c4c4"
                        }
                    }

                    contentItem: Text {
                        text: naturalButton.text
                        font: naturalButton.font
                        color: naturalButton.checked ? "white" : "#3b3b3b"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        checked = true
                        samplesButton.checked = false
                        root.naturalUnits = true
                    }
                }

                Button {
                    id: samplesButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    checkable: true
                    checked: false
                    text: "Samples"
                    font.family: root.labelsFontName
                    font.pixelSize: root.fontSize - 2

                    background: Rectangle {
                        color: samplesButton.checked ? "#c4c4c4" : "transparent"
                        radius: 3
                        Rectangle {
                            width: 2
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            color: "#c4c4c4"
                        }
                    }

                    contentItem: Text {
                        text: samplesButton.text
                        font: samplesButton.font
                        color: samplesButton.checked ? "white" : "#3b3b3b"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        checked = true
                        naturalButton.checked = false
                        root.naturalUnits = false
                    }
                }
            }
        }

        // Spacer
        Item {
            Layout.fillWidth: true
        }

        // Coordinates Display
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: root.height - 10
            Layout.alignment: Qt.AlignVCenter
            color: "#d0d0d0"
        }

        Label {
            text: "X:"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
            color: "#606060"
        }

        Label {
            id: xCoordLabel
            Layout.preferredWidth: 60
            Layout.alignment: Qt.AlignVCenter
            text: "0"
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
            horizontalAlignment: Text.AlignRight

            Connections {
                target: chartMouseArea
                function onCursorX(x) {
                    xCoordLabel.text = x
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: root.height - 10
            Layout.alignment: Qt.AlignVCenter
            color: "#d0d0d0"
        }

        Label {
            text: "Y:"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
            color: "#606060"
        }

        Label {
            id: yCoordLabel
            Layout.preferredWidth: 60
            Layout.alignment: Qt.AlignVCenter
            text: "0"
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
            horizontalAlignment: Text.AlignRight

            Connections {
                target: chartMouseArea
                function onCursorY(y) {
                    yCoordLabel.text = y
                }
            }
        }
    }
}
