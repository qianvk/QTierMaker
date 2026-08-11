#include "app/Application.h"

#include <QCoreApplication>

int main(int argc, char* argv[]) {
    // VkUI must keep the frameless host as the only native widget window. Creating a
    // transient popover must not promote title-bar siblings outside the agent's event pipeline.
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    qtm::Application app(argc, argv);
    return app.run();
}
