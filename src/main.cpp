#include "app/application.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    CrashEdit::Application app;
    return app.run();
}
#else
int main() {
    CrashEdit::Application app;
    return app.run();
}
#endif
