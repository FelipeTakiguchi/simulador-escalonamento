// ============================================================================
// gantt_janela.cpp - Janela do Windows que desenha o Gantt (usa só a API do próprio
// Windows, sem bibliotecas extras). Fora do Windows, abrirJanela() devolve false.
// A janela mostra os últimos ticks que cabem na tela e acompanha o tick atual.
// ============================================================================
#include "header/gantt.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include <algorithm>

namespace gantt {
namespace {

const Controles* ctl = nullptr;  // a função de eventos do Windows não recebe parâmetros extras

COLORREF rgb(const std::string& h) {
    return RGB(std::stoi(h.substr(0, 2), nullptr, 16), std::stoi(h.substr(2, 2), nullptr, 16), std::stoi(h.substr(4, 2), nullptr, 16));
}

std::wstring larga(const std::string& s) {  // UTF-8 -> UTF-16 (o Windows desenha UTF-16)
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 1 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

void desenha(HDC dc, const Forma& f) {
    const int x = (int)f.x, y = (int)f.y, w = (int)f.w, h = (int)f.h;
    if (f.tipo == TipoForma::TEXTO) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, rgb(f.cor));
        SetTextAlign(dc, TA_TOP | (f.alinha == 0 ? TA_LEFT : f.alinha == 1 ? TA_CENTER : TA_RIGHT));
        std::wstring t = larga(f.texto);
        TextOutW(dc, x, y - 8, t.c_str(), (int)t.size());  // y é o meio da linha de texto
    } else if (f.tipo == TipoForma::LINHA) {
        SelectObject(dc, GetStockObject(DC_PEN));
        SetDCPenColor(dc, rgb(f.cor));
        MoveToEx(dc, x, y, nullptr);
        LineTo(dc, x + w, y + h);
    } else {  // RETANGULO ou PONTILHADO
        SelectObject(dc, GetStockObject(BLACK_PEN));
        SelectObject(dc, GetStockObject(DC_BRUSH));
        SetDCBrushColor(dc, f.tipo == TipoForma::PONTILHADO ? RGB(255, 255, 255) : rgb(f.cor));
        Rectangle(dc, x, y, x + w, y + h);
        if (f.tipo == TipoForma::PONTILHADO)  // pontos pretos
            for (int yy = y + 3; yy < y + h - 2; yy += 5)
                for (int xx = x + 3; xx < x + w - 2; xx += 5) {
                    RECT p = {xx, yy, xx + 2, yy + 2};
                    FillRect(dc, &p, (HBRUSH)GetStockObject(BLACK_BRUSH));
                }
    }
}

LRESULT CALLBACK tratar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int ate = ctl->tickAtual();
        int cabem = std::max(1, (int)(rc.right - MARG_ESQ - 10) / LARG_TICK);  // quantos ticks cabem na janela
        Layout L = montarLayout(ctl->dados(), ate, std::max(0, ate - cabem));
        HFONT fonte = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HGDIOBJ antiga = SelectObject(dc, fonte);
        for (const Forma& f : L.formas) desenha(dc, f);
        SelectObject(dc, antiga);
        DeleteObject(fonte);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RIGHT) ctl->avancar();
        else if (wp == VK_LEFT) ctl->voltar();
        else if (wp == VK_END) ctl->executarTudo();
        else if (wp == 'S') MessageBoxW(hwnd, salvarSvg(ctl->dados(), "gantt.svg") ? L"gantt.svg salvo na pasta atual." : L"Erro ao salvar.", L"SVG", MB_OK);
        std::wstring titulo = L"Simulador - tick " + std::to_wstring(ctl->tickAtual()) + L"   [seta direita: avançar | seta esquerda: voltar | End: fim | S: salvar SVG]";
        SetWindowTextW(hwnd, titulo.c_str());
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool abrirJanela(const Controles& c) {
    ctl = &c;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = tratar;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"GanttSimulador";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Simulador", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 480, nullptr, nullptr, inst, nullptr);
    if (!hwnd) return false;
    SendMessageW(hwnd, WM_KEYDOWN, 0, 0);  // só para atualizar o título
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

}  // namespace gantt

#else

namespace gantt {
bool abrirJanela(const Controles&) { return false; }
}  // namespace gantt

#endif
