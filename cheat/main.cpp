// language: C++, file: cheat/main.cpp, target: Windows 11 x64, MSVC
// ESP client entry point
// Flow: init driver → find game → overlay window → render loop

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>

#include "memory.hpp"
#include "esp.hpp"
#include "overlay.hpp"

// ── Get primary monitor resolution ──────────────────────────────────────────
static void GetScreenSize(float& w, float& h)
{
    w = (float)GetSystemMetrics(SM_CXSCREEN);
    h = (float)GetSystemMetrics(SM_CYSCREEN);
}

// ── ESP logic thread: reads entities ~200Hz ──────────────────────────────────
static bool g_esp_running = false;

static void EspThread()
{
    float sw, sh;
    GetScreenSize(sw, sh);

    while (g_esp_running) {
        auto t0 = std::chrono::high_resolution_clock::now();

        esp_reader::UpdateFrame(sw, sh);

        // throttle to ~200 fps (5ms) — fast enough to feel smooth
        auto t1 = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        if (elapsed < 5000)
            std::this_thread::sleep_for(std::chrono::microseconds(5000 - elapsed));
    }
}

// ── Entry point ──────────────────────────────────────────────────────────────
int main()
{
    // Hide console in release build (comment out for debugging)
    // ShowWindow(GetConsoleWindow(), SW_HIDE);

    std::cout << "[*] VRD ESP starting\n";

    // ── 1. Connect to kernel driver ──────────────────────────────────────────
    if (!mem::Init()) {
        std::cerr << "[-] driver not found — run loader.exe first\n";
        system("pause");
        return 1;
    }

    // ── 2. Find Valorant ─────────────────────────────────────────────────────
    std::cout << "[*] waiting for VALORANT-Win64-Shipping.exe...\n";
    while (!mem::FindGame(L"VALORANT-Win64-Shipping.exe")) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::cout << "[+] game found\n";

    // Brief pause — let Vanguard settle before we start reading
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // ── 3. Create overlay ────────────────────────────────────────────────────
    float sw, sh;
    GetScreenSize(sw, sh);

    if (!overlay::Init(sw, sh)) {
        std::cerr << "[-] overlay init failed\n";
        mem::Shutdown();
        return 1;
    }
    std::cout << "[+] overlay up — INSERT to open menu, END to exit\n";

    // ── 4. Start ESP reader thread ───────────────────────────────────────────
    g_esp_running = true;
    std::thread esp_t(EspThread);

    // ── 5. Main render loop (UI thread) ──────────────────────────────────────
    while (overlay::PumpMessages()) {
        // EXIT on END key
        if (GetAsyncKeyState(VK_END) & 0x8000) break;

        overlay::RenderFrame();
    }

    // ── 6. Cleanup ───────────────────────────────────────────────────────────
    g_esp_running = false;
    if (esp_t.joinable()) esp_t.join();

    overlay::Shutdown();
    mem::Shutdown();

    std::cout << "[+] clean exit\n";
    return 0;
}
