import QtQuick

Window {
    id: root
    width: 1100
    height: 720
    minimumWidth: 800
    minimumHeight: 520
    visible: true
    title: qsTr("Morze")
    color: "#0b1219"

    property int currentTab: 0
    property bool groupInfoOpen: false

    Connections {
        target: clientBridge
        function onSelectedChatIdChanged() {
            groupInfoOpen = false
        }
        function onErrorOccurred(message) {
            console.warn(message)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0e1621"

        Row {
            anchors.fill: parent
            spacing: 0

            // Левая вертикальная панель — вкладки
            Rectangle {
                width: 56
                height: parent.height
                color: "#0b1219"
                border.color: "#1c2836"

                Column {
                    width: parent.width
                    spacing: 4
                    topPadding: 12

                    Repeater {
                        // Числовая модель: у JS-массива объектов роли в делегате часто не видны — картинки не грузятся
                        model: 2

                        Rectangle {
                            width: 56
                            height: 52
                            color: currentTab === index ? "#1a2d42" : "transparent"
                            radius: 8

                            Column {
                                anchors.centerIn: parent
                                spacing: 3
                                Image {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 22
                                    height: 22
                                    source: Qt.resolvedUrl(index === 0 ? "img/chats.svg" : "img/sets.png")
                                    sourceSize: Qt.size(44, 44)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    opacity: currentTab === index ? 1 : 0.5
                                }
                                Text {
                                    text: index === 0 ? qsTr("Чаты") : qsTr("Настройки")
                                    color: currentTab === index ? "#d7e6ff" : "#7a8fa3"
                                    font.pixelSize: 9
                                    horizontalAlignment: Text.AlignHCenter
                                    width: 56
                                    wrapMode: Text.Wrap
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    currentTab = index
                                    if (index === 1)
                                        groupInfoOpen = false
                                }
                            }
                        }
                    }
                }

                Item {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 12
                    width: 36
                    height: 36
                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: "#2a4a6e"
                        Image {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            source: Qt.resolvedUrl("img/logo.svg")
                            sourceSize: Qt.size(44, 44)
                            fillMode: Image.PreserveAspectFit
                        }
                    }
                }
            }

            // Основная зона
            Loader {
                id: mainLoader
                width: parent.width - 56
                height: parent.height
                active: true
                sourceComponent: currentTab === 0 ? chatsView : settingsView
            }
        }
    }

    Component {
        id: chatsView

        Row {
            anchors.fill: parent
            spacing: 0

            // Список чатов
            Rectangle {
                width: 300
                height: parent.height
                color: "#111b26"
                border.color: "#1c2836"

                Column {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        width: parent.width
                        height: 52
                        color: "#111b26"
                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Image {
                                width: 24
                                height: 24
                                source: Qt.resolvedUrl("img/chats.svg")
                                sourceSize: Qt.size(48, 48)
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Чаты"
                                color: "#e6eef8"
                                font.pixelSize: 18
                                font.bold: true
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width - 24
                        height: 36
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 18
                        color: "#1a2633"
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 12
                            spacing: 6
                            Image {
                                width: 18
                                height: 18
                                anchors.verticalCenter: parent.verticalCenter
                                source: Qt.resolvedUrl("img/search.svg")
                                sourceSize: Qt.size(36, 36)
                                fillMode: Image.PreserveAspectFit
                                opacity: 0.85
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Поиск…"
                                color: "#5c6d80"
                                font.pixelSize: 13
                            }
                        }
                    }

                    // отступ под строкой поиска до списка чатов
                    Item {
                        width: parent.width
                        height: 14
                    }

                    ListView {
                        id: chatListView
                        width: parent.width
                        height: parent.height - 52 - 36 - 14 - 12
                        clip: true
                        model: clientBridge.chatModel
                        spacing: 0

                        delegate: Rectangle {
                            width: chatListView.width
                            height: 64
                            color: ListView.isCurrentItem ? "#1e3a5c" : (mouse.pressed ? "#162433" : "transparent")

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10

                                Rectangle {
                                    width: 44
                                    height: 44
                                    radius: 22
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Qt.hsla(avatarHue / 360, 0.35, 0.42, 1)
                                    Text {
                                        anchors.centerIn: parent
                                        text: avatarLetter
                                        color: "#f0f6ff"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                }

                                Column {
                                    width: parent.width - 54 - 44
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Row {
                                        width: parent.width
                                        Text {
                                            width: parent.width - 52
                                            text: title
                                            color: "#e6eef8"
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: time
                                            color: "#6b7f94"
                                            font.pixelSize: 11
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        text: lastText
                                        color: "#8fa3b8"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle {
                                    visible: unread > 0
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: "#3d7bcd"
                                    anchors.verticalCenter: parent.verticalCenter
                                    Text {
                                        anchors.centerIn: parent
                                        text: unread
                                        color: "#fff"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }

                            MouseArea {
                                id: mouse
                                anchors.fill: parent
                                onClicked: {
                                    chatListView.currentIndex = index
                                    clientBridge.selectChatByIndex(index)
                                }
                            }
                        }

                        Component.onCompleted: {
                            if (count > 0) {
                                currentIndex = 0
                                clientBridge.selectChatByIndex(0)
                            }
                        }
                    }
                }
            }

            // Окно чата
            Rectangle {
                width: parent.width - 300
                height: parent.height
                color: "#0e1621"

                Column {
                    anchors.fill: parent
                    spacing: 0

                    // Шапка чата
                    Rectangle {
                        width: parent.width
                        height: 52
                        color: "#152232"
                        border.color: "#1c2836"

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 10

                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                anchors.verticalCenter: parent.verticalCenter
                                color: Qt.hsla(clientBridge.selectedAvatarHue / 360, 0.35, 0.42, 1)
                                Text {
                                    anchors.centerIn: parent
                                    text: clientBridge.selectedAvatarLetter
                                    color: "#f0f6ff"
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                            }

                            Column {
                                width: parent.width - 40 - 44 - 16
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text {
                                    width: parent.width
                                    text: clientBridge.selectedTitle
                                    color: "#e6eef8"
                                    font.pixelSize: 15
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: clientBridge.selectedSubtitle
                                    color: "#7a8fa3"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                width: 36
                                height: 36
                                radius: 8
                                color: infoMa.containsMouse ? "#1e3a5c" : "transparent"
                                anchors.verticalCenter: parent.verticalCenter
                                Image {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    source: Qt.resolvedUrl("img/info.svg")
                                    sourceSize: Qt.size(44, 44)
                                    fillMode: Image.PreserveAspectFit
                                    opacity: infoMa.containsMouse ? 1 : 0.9
                                }
                                MouseArea {
                                    id: infoMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: groupInfoOpen = !groupInfoOpen
                                }
                            }
                        }
                    }

                    // Лента сообщений
                    ListView {
                        id: msgList
                        width: parent.width
                        height: parent.height - 52 - 56
                        clip: true
                        spacing: 8
                        model: clientBridge.messageModel
                        verticalLayoutDirection: ListView.TopToBottom

                        onCountChanged: if (count > 0)
                            scrollEndTimer.restart()

                        Component.onCompleted: scrollEndTimer.restart()

                        Timer {
                            id: scrollEndTimer
                            interval: 0
                            repeat: false
                            onTriggered: if (msgList.count > 0)
                                msgList.positionViewAtEnd()
                        }

                        delegate: Item {
                            width: msgList.width
                            height: bubbleCol.height

                            Column {
                                id: bubbleCol
                                width: Math.min(msgList.width * 0.78, 520)
                                x: model.isOutgoing ? (msgList.width - width - 12) : 12
                                spacing: 4

                                Text {
                                    visible: !model.isOutgoing && model.name.length > 0
                                    text: model.name
                                    color: "#6b9bd1"
                                    font.pixelSize: 11
                                }

                                Rectangle {
                                    width: parent.width
                                    radius: 12
                                    color: model.isOutgoing ? "#2b5278" : "#1a2633"
                                    height: bubbleText.height + bubbleRow.height + 20

                                    Column {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            id: bubbleText
                                            width: parent.width
                                            text: model.text
                                            wrapMode: Text.Wrap
                                            color: "#e6eef8"
                                            font.pixelSize: 13
                                        }
                                        Row {
                                            id: bubbleRow
                                            width: parent.width
                                            Text {
                                                text: model.time
                                                color: model.isOutgoing ? "#b8d4f0" : "#7a8fa3"
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 56
                        color: "#111b26"
                        border.color: "#1c2836"
                        Row {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8
                            Rectangle {
                                height: parent.height
                                width: parent.width - 48
                                radius: 10
                                color: "#1a2633"
                                TextInput {
                                    id: messageInput
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    color: "#e6eef8"
                                    font.pixelSize: 13
                                    selectByMouse: true
                                    clip: true
                                    focus: true
                                    onAccepted: {
                                        clientBridge.sendMessage(text)
                                        clear()
                                    }
                                }
                            }
                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                color: "#2a4a6e"
                                Image {
                                    anchors.centerIn: parent
                                    width: 20
                                    height: 20
                                    source: Qt.resolvedUrl("img/send.svg")
                                    sourceSize: Qt.size(48, 48)
                                    fillMode: Image.PreserveAspectFit
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        clientBridge.sendMessage(messageInput.text)
                                        messageInput.clear()
                                    }
                                }
                            }
                        }
                    }
                }

                // Панель информации о группе
                Rectangle {
                    visible: groupInfoOpen
                    anchors.fill: parent
                    color: "#99000000"
                    z: 50
                    MouseArea {
                        anchors.fill: parent
                        onClicked: groupInfoOpen = false
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 320
                        color: "#152232"
                        border.color: "#1c2836"

                        MouseArea {
                            anchors.fill: parent
                            // поглощаем клики, чтобы не закрывать
                        }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 0
                            spacing: 0

                            Rectangle {
                                width: parent.width
                                height: 48
                                color: "#152232"
                                Row {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 8
                                    Image {
                                        width: 22
                                        height: 22
                                        source: Qt.resolvedUrl("img/info.svg")
                                        sourceSize: Qt.size(44, 44)
                                        fillMode: Image.PreserveAspectFit
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: "Информация"
                                        color: "#e6eef8"
                                        font.pixelSize: 15
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Rectangle {
                                    width: 36
                                    height: 36
                                    radius: 8
                                    color: closeMa.containsMouse ? "#1e3a5c" : "transparent"
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    Image {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        source: Qt.resolvedUrl("img/close.svg")
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        opacity: closeMa.containsMouse ? 1 : 0.85
                                    }
                                    MouseArea {
                                        id: closeMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: groupInfoOpen = false
                                    }
                                }
                            }

                            Flickable {
                                id: infoFlick
                                width: parent.width
                                height: parent.height - 48
                                contentWidth: width
                                contentHeight: infoColumn.height
                                clip: true

                                Column {
                                    id: infoColumn
                                    width: infoFlick.width
                                    spacing: 0
                                    topPadding: 20

                                    Rectangle {
                                        width: 72
                                        height: 72
                                        radius: 36
                                        color: Qt.hsla(clientBridge.selectedAvatarHue / 360, 0.35, 0.42, 1)
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        Text {
                                            anchors.centerIn: parent
                                            text: clientBridge.selectedAvatarLetter
                                            color: "#f0f6ff"
                                            font.pixelSize: 28
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        width: parent.width - 32
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        text: clientBridge.selectedTitle
                                        wrapMode: Text.Wrap
                                        color: "#e6eef8"
                                        font.pixelSize: 17
                                        font.bold: true
                                        topPadding: 12
                                    }

                                    Text {
                                        width: parent.width - 32
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        text: clientBridge.selectedSubtitle
                                        color: "#7a8fa3"
                                        font.pixelSize: 12
                                        topPadding: 4
                                    }

                                    Item {
                                        width: parent.width
                                        height: 20
                                    }
                                    Rectangle {
                                        width: parent.width - 24
                                        height: 1
                                        color: "#1c2836"
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }

                                    Text {
                                        width: parent.width - 32
                                        x: 16
                                        text: "Описание"
                                        color: "#6b7f94"
                                        font.pixelSize: 11
                                        topPadding: 16
                                    }
                                    Text {
                                        width: parent.width - 32
                                        x: 16
                                        wrapMode: Text.Wrap
                                        text: "Описание пока не подключено к серверному профилю."
                                        color: "#b8cfe8"
                                        font.pixelSize: 13
                                        topPadding: 6
                                    }

                                    Text {
                                        width: parent.width - 32
                                        x: 16
                                        text: "Участники"
                                        color: "#6b7f94"
                                        font.pixelSize: 11
                                        topPadding: 18
                                    }
                                    Text {
                                        width: parent.width - 32
                                        x: 16
                                        wrapMode: Text.Wrap
                                        text: "Список участников будет подключен следующим шагом через CommunicationController."
                                        color: "#8fa3b8"
                                        font.pixelSize: 12
                                        topPadding: 8
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: settingsView

        Rectangle {
            anchors.fill: parent
            color: "#0e1621"

            Column {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                Row {
                    spacing: 10
                    Image {
                        width: 28
                        height: 28
                        source: Qt.resolvedUrl("img/sets.png")
                        sourceSize: Qt.size(56, 56)
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Настройки"
                        color: "#e6eef8"
                        font.pixelSize: 22
                        font.bold: true
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Профиль сети читает реальные значения из ConnectionProfileService."
                    color: "#8fa3b8"
                    font.pixelSize: 13
                }

                Text {
                    text: "Сеть"
                    color: "#e6eef8"
                    font.pixelSize: 15
                    font.bold: true
                }

                Column {
                    width: Math.min(parent.width, 520)
                    spacing: 14

                    Column {
                        width: parent.width
                        spacing: 6
                        Text {
                            text: "IP-адрес (bind)"
                            color: "#7a8fa3"
                            font.pixelSize: 11
                        }
                        Row {
                            width: parent.width
                            spacing: 8
                            Rectangle {
                                width: parent.width - 8 - 42
                                height: 38
                                radius: 10
                                color: "#1a2633"
                                border.color: "#243447"
                                TextInput {
                                    id: bindIpField
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    color: "#e6eef8"
                                    font.pixelSize: 13
                                    selectByMouse: true
                                    clip: true
                                    Component.onCompleted: text = clientBridge.netBindAddress
                                    onEditingFinished: clientBridge.updateNetworkSettings(text.trim(), stunField.text.trim(), relayField.text.trim())
                                }
                            }
                            Rectangle {
                                width: 42
                                height: 38
                                radius: 10
                                color: bindRefreshMa.containsMouse ? "#345a85" : "#2a4a6e"
                                border.color: "#3d5a80"
                                Image {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    source: Qt.resolvedUrl("img/refresh.svg")
                                    sourceSize: Qt.size(44, 44)
                                    fillMode: Image.PreserveAspectFit
                                    opacity: bindRefreshMa.containsMouse ? 1 : 0.92
                                }
                                MouseArea {
                                    id: bindRefreshMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // сюда можно добавить автопоиск доступного сетевого интерфейса
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        Text {
                            text: "STUN"
                            color: "#7a8fa3"
                            font.pixelSize: 11
                        }
                        Row {
                            width: parent.width
                            spacing: 8
                            Rectangle {
                                width: parent.width - 8 - 42
                                height: 38
                                radius: 10
                                color: "#1a2633"
                                border.color: "#243447"
                                TextInput {
                                    id: stunField
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    color: "#e6eef8"
                                    font.pixelSize: 13
                                    selectByMouse: true
                                    clip: true
                                    Component.onCompleted: text = clientBridge.netStunServer
                                    onEditingFinished: clientBridge.updateNetworkSettings(bindIpField.text.trim(), text.trim(), relayField.text.trim())
                                }
                            }
                            Rectangle {
                                width: 42
                                height: 38
                                radius: 10
                                color: stunPingMa.containsMouse ? "#345a85" : "#2a4a6e"
                                border.color: "#3d5a80"
                                Image {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    source: Qt.resolvedUrl("img/ping.svg")
                                    sourceSize: Qt.size(44, 44)
                                    fillMode: Image.PreserveAspectFit
                                    opacity: stunPingMa.containsMouse ? 1 : 0.92
                                }
                                MouseArea {
                                    id: stunPingMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                    }
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 6
                        Text {
                            text: "Relay (TURN)"
                            color: "#7a8fa3"
                            font.pixelSize: 11
                        }
                        Row {
                            width: parent.width
                            spacing: 8
                            Rectangle {
                                width: parent.width - 8 - 42
                                height: 38
                                radius: 10
                                color: "#1a2633"
                                border.color: "#243447"
                                TextInput {
                                    id: relayField
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    color: "#e6eef8"
                                    font.pixelSize: 13
                                    selectByMouse: true
                                    clip: true
                                    Component.onCompleted: text = clientBridge.netRelayServer
                                    onEditingFinished: clientBridge.updateNetworkSettings(bindIpField.text.trim(), stunField.text.trim(), text.trim())
                                }
                            }
                            Rectangle {
                                width: 42
                                height: 38
                                radius: 10
                                color: relayPingMa.containsMouse ? "#345a85" : "#2a4a6e"
                                border.color: "#3d5a80"
                                Image {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    source: Qt.resolvedUrl("img/ping.svg")
                                    sourceSize: Qt.size(44, 44)
                                    fillMode: Image.PreserveAspectFit
                                    opacity: relayPingMa.containsMouse ? 1 : 0.92
                                }
                                MouseArea {
                                    id: relayPingMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
