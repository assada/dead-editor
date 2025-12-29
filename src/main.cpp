#include "Application.h"
#include "PlatformInit.h"
#include <iostream>

int main(int argc, char* argv[]) {
    platform_init();

    try {
        Application app(argc, argv);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown Fatal Error" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
