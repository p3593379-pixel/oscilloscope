import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    height: 28
    color: "#ffffff"
    border.width: 0
    property string labelsFontName: "monospace"
    readonly property int fontSize: Math.max(8, Math.round(root.height * 12 / 28))
    property var oscView: null
    property bool naturalUnits: false

    signal moveLeft(real x)
    signal moveRight(real x)

    function setSbStart(x) {
        xStartSpinBox.value = x
    }

    function setSbShow(x) {
        xShowSpinBox.value = x
    }

    RowLayout {
        id: rowLayout
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        Label {
            id: xStartLabel
            text: "X start:"
            Layout.alignment: Qt.AlignVCenter
            font.family: root.labelsFontName
            font.pixelSize: root.fontSize
        }

        SpinBox {
            id: xStartSpinBox
            Layout.minimumWidth: 80
            Layout.preferredWidth: 80
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            value: 0
            editable: true
            from: -1048576
            to: 1048576

            contentItem: TextInput {
                text: xStartSpinBox.textFromValue(xStartSpinBox.value, xStartSpinBox.locale)
                font.family: root.labelsFontName
                font.pixelSize: root.fontSize
                color: "#2c2c2c"
                leftPadding: 8
                rightPadding: 0
                horizontalAlignment: Qt.AlignLeft
                verticalAlignment: Qt.AlignVCenter
                readOnly: !xStartSpinBox.editable
                validator: xStartSpinBox.validator
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            
            onValueModified: {
                if (root.oscView) {
                    var currentRange = root.oscView.getXMax() - root.oscView.getXMin()
                    root.oscView.setXMin(xStartSpinBox.value)
                    root.oscView.setXMax(xStartSpinBox.value + currentRange)
                }
            }
        }

        Label {
            id: xStartLabelUnits
            text: root.naturalUnits ? "m" : "smps"
            Layout.alignment: Qt.AlignVCenter
            font.family: root.labelsFontName
            font.pixelSize: root.fontSize
        }

        Item {
            Layout.fillWidth: true
        }

        Label {
            id: showPointsLabel
            text: "Show :"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
        }

        SpinBox {
            id: xShowSpinBox
            Layout.minimumWidth: 80
            Layout.preferredWidth: 80
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            value: 8192
            editable: true
            from: 1
            to: 1048576

            contentItem: TextInput {
                text: xShowSpinBox.textFromValue(xShowSpinBox.value, xShowSpinBox.locale)
                font.family: root.labelsFontName
                font.pixelSize: root.fontSize
                color: "#2c2c2c"
                leftPadding: 8
                rightPadding: 0
                horizontalAlignment: Qt.AlignLeft
                verticalAlignment: Qt.AlignVCenter
                readOnly: !xShowSpinBox.editable
                validator: xShowSpinBox.validator
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            onValueModified: {
                if (root.oscView) {
                    root.oscView.setXMax(root.oscView.getXMin() + xShowSpinBox.value)
                }
            }
        }

        Label {
            id: showPointsUnitsLabel
            text: root.naturalUnits ? "m" : "smps"
            Layout.alignment: Qt.AlignVCenter
            font.family: root.labelsFontName
            font.pixelSize: root.fontSize
        }

        Item {
            Layout.fillWidth: true
        }

        Label {
            id: moveByLabel
            text: "Move by:"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
        }

        Button {
            id: moveLeftButton
            Layout.preferredHeight: root.height - 4
            Layout.preferredWidth: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            icon.source: "qrc:/img/moveLeft.svg"
            icon.width: root.height - 6
            icon.height: root.height - 6
            flat: true

            onClicked: {
                root.moveLeft(xStepComboBox.currentValue)
            }
        }

        ComboBox {
            id: xStepComboBox
            Layout.minimumWidth: 80
            Layout.preferredWidth: 80
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            editable: false
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
            currentIndex: 6

            model: /*root.naturalUnits ? ListModel {
                ListElement { text: "0.04"; value: 0.04 }
                ListElement { text: "0.08"; value: 0.08 }
                ListElement { text: "0.16"; value: 0.16 }
                ListElement { text: "0.32"; value: 0.32 }
                ListElement { text: "0.64"; value: 0.64 }
                ListElement { text: "1.28"; value: 1.28 }
                ListElement { text: "2.56"; value: 2.56 }
                ListElement { text: "5.12"; value: 5.12 }
                ListElement { text: "10.24"; value: 10.24 }
                ListElement { text: "20.48"; value: 20.48 }
                ListElement { text: "40.96"; value: 40.96 }
            } : */ ListModel {
                ListElement { text: "16"; value: 16 }
                ListElement { text: "32"; value: 32 }
                ListElement { text: "64"; value: 64 }
                ListElement { text: "128"; value: 128 }
                ListElement { text: "256"; value: 256 }
                ListElement { text: "512"; value: 512 }
                ListElement { text: "1024"; value: 1024 }
                ListElement { text: "2048"; value: 2048 }
                ListElement { text: "4096"; value: 4096 }
                ListElement { text: "8192"; value: 8192 }
                ListElement { text: "16384"; value: 16384 }
                ListElement { text: "32768"; value: 32768 }
                ListElement { text: "65536"; value: 65536 }
            }

            textRole: "text"
            valueRole: "value"

            contentItem: Text {
                text: xStepComboBox.displayText
                font.family: root.labelsFontName
                font.pixelSize: root.fontSize - 1
                color: "#2c2c2c"
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
                rightPadding: 0
                elide: Text.ElideRight
            }
            delegate: ItemDelegate {
                width: xStepComboBox.width

                contentItem: Text {
                    text: model.text
                    font.family: root.labelsFontName
                    font.pixelSize: root.fontSize - 1
                    color: "#2c2c2c"
                    verticalAlignment: Text.AlignVCenter
                }

                highlighted: xStepComboBox.highlightedIndex === index
            }

            onActivated: {
                if (root.oscView) {
                    xShowSpinBox.value = currentValue
                    root.oscView.setXMax(root.oscView.getXMin() + xShowSpinBox.value)
                }
            }
        }

        Label {
            id: moveByUnitsLabel
            text: root.naturalUnits ? "m" : "smps"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
        }

        Button {
            id: moveRightButton
            Layout.preferredHeight: root.height - 4
            Layout.preferredWidth: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            icon.source: "qrc:/img/moveRight.svg"
            icon.width: root.height - 6
            icon.height: root.height - 6
            flat: true

            onClicked: {
                root.moveRight(xStepComboBox.currentValue)
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Label {
            id: yPeakLabel
            text: "Y peak-to-peak:"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
        }

        SpinBox {
            id: yPeakToPeakSpinBox
            Layout.preferredWidth: 80
            Layout.preferredHeight: root.height - 4
            Layout.alignment: Qt.AlignVCenter
            value: 32768
            editable: true
            from: 1
            to: 1048576

            contentItem: TextInput {
                text: yPeakToPeakSpinBox.textFromValue(yPeakToPeakSpinBox.value, yPeakToPeakSpinBox.locale)
                font.family: root.labelsFontName
                font.pixelSize: root.fontSize
                color: "#2c2c2c"
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                readOnly: !yPeakToPeakSpinBox.editable
                validator: yPeakToPeakSpinBox.validator
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            onValueModified: {
                if (root.oscView) {
                    root.oscView.setYMax(yPeakToPeakSpinBox.value)
                    root.oscView.setYMin(-yPeakToPeakSpinBox.value)
                }
            }
        }

        Label {
            id: yPeakUnitsLabel
            text: root.naturalUnits ? "smps" : "smps"
            Layout.alignment: Qt.AlignVCenter
            font.pixelSize: root.fontSize
            font.family: root.labelsFontName
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
