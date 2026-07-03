#ifndef LEXER_GEN_H
#define LEXER_GEN_H

#include "automata.h"
#include <string>
#include <map>

std::string generate_lexer_cpp(DFA* min_dfa, const std::map<std::string, int>& keywords);

#endif // LEXER_GEN_H
