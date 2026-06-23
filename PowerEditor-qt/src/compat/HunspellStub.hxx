#pragma once
// Stub de Hunspell para plataformas sem a biblioteca (ex.: build Windows MinGW,
// onde não há mingw64-hunspell). Mantém a API mínima usada por SpellChecker:
// toda palavra é tratada como correta e não há sugestões — ou seja, o corretor
// ortográfico vira um no-op silencioso, sem espalhar #ifdef pelo código.
//
// Quando um Hunspell nativo para Windows for adicionado, basta remover
// LORDPAD_NO_HUNSPELL do build e este stub deixa de ser incluído.
#include <string>
#include <vector>

class Hunspell {
public:
    Hunspell(const char* /*affpath*/, const char* /*dicpath*/) {}
    int spell(const std::string& /*word*/) { return 1; }   // 1 = correta
    std::vector<std::string> suggest(const std::string& /*word*/) { return {}; }
};
