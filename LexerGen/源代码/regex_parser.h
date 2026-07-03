#ifndef REGEX_PARSER_H
#define REGEX_PARSER_H

#include "automata.h"
#include <string>
#include <map>

struct LexerDef {
    NFA* global_nfa;
    std::map<std::string, int> keywords;
};

LexerDef parse_lexer_definitions(const std::string& filepath);

#endif // REGEX_PARSER_H
