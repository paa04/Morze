#include <QCoreApplication>
#include <QApplication>

#include "CommunicationController.h"
#include "DependencyContainer.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    try {
        DependencyContainer container("config.json");

        // Получаем контроллер
        auto commController = container.communicationController();

        // Создаём главное окно, передаём ему сервисы из контейнера (если нужно)
        // MainWindow window(container.chatService(), container.messageService(), commController);
        // window.show();

        return app.exec();
    } catch (const std::exception& e) {
        // обработка ошибок
        return 1;
    }
}
