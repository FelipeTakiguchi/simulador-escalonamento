#pragma once
// ============================================================================
// util.hpp - funções pequenas de texto usadas por várias partes do simulador.
// Ficam num header (inline) para não exigir mais um .cpp só para isso.
// ============================================================================
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// Remove espaços/tabs/quebras de linha das pontas (espaços não são erro - req. 3.3.6).
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Comparações de texto não diferenciam maiúsculas/minúsculas (req. 3.3.2):
// normalizamos tudo para maiúsculas antes de comparar.
inline std::string maiusculas(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
inline std::string minusculas(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Divide por 'sep' MANTENDO campos vazios ("a;;b" -> {"a","","b"}), pois campo vazio
// significa "use o valor padrão" (req. 3.2).
inline std::vector<std::string> divide(const std::string& s, char sep) {
    std::vector<std::string> r;
    std::string atual;
    for (char c : s) {
        if (c == sep) { r.push_back(atual); atual.clear(); }
        else atual += c;
    }
    r.push_back(atual);
    return r;
}

// Converte texto em inteiro de forma ESTRITA: só aceita [+-]dígitos (sem "12abc", sem "1.5").
inline bool paraInt(const std::string& txt, int& out) {
    std::string s = trim(txt);
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i == s.size() || s.size() - i > 9) return false;  // vazio ou grande demais (evita overflow)
    for (size_t k = i; k < s.size(); ++k)
        if (!std::isdigit((unsigned char)s[k])) return false;
    out = std::stoi(s);
    return true;
}

inline std::string juntaLinhas(const std::vector<std::string>& v) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) r += (i ? "\n" : "") + v[i];
    return r;
}
