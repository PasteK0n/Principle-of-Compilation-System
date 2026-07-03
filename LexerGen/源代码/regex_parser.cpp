/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        regex_parser.cpp
 * @brief       用于实现正则表达式解析的模块，支持宏定义、正则表达式语法解析以及NFA构建
 *
 * (3) 当前版本号，作者/修改者，完成日期
 * @version     V1.0.0
 * @author      陈乐豪
 * @date        2026-04-01
 *
 * (4) 版本历史信息 (Revision History)
 * -------------------------------------------------------------------------
 * Version     Date          Author          Description
 * V1.0.0      2026-04-01    陈乐豪      1. 创建文件并初始化基础功能
 ******************************************************************************/
#include "regex_parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>

using namespace std;

map<string, string> macros;

void trim(string& s) {
    size_t p = s.find_first_not_of(" \t\r\n");
    if(p == string::npos) { s = ""; return; }
    s.erase(0, p);
    p = s.find_last_not_of(" \t\r\n");
    if(string::npos != p) s.erase(p+1);
}

vector<string> split_top_level(const string& s) {
    vector<string> res;
    string cur;
    int depth = 0;
    for(size_t pos = 0; pos < s.length(); pos++) {
        char c = s[pos];
        if (c == '\\') {
            cur += c;
            if(pos + 1 < s.length()) { cur += s[pos+1]; pos++; }
        } else if (c == '(') {
            depth++;
            cur += c;
        } else if (c == ')') {
            depth--;
            cur += c;
        } else if (c == '|' && depth == 0) {
            res.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    res.push_back(cur);
    return res;
}

string expand_macros(string pattern) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto const& pair : macros) {
            const string& name = pair.first;
            const string& val = pair.second;
            size_t pos = 0;
            while ((pos = pattern.find(name, pos)) != string::npos) {
                bool left_bound = (pos == 0) || !isalnum(pattern[pos - 1]);
                bool right_bound = (pos + name.length() == pattern.length()) || !isalnum(pattern[pos + name.length()]);
                if (left_bound && right_bound) {
                    pattern.replace(pos, name.length(), "(" + val + ")");
                    pos += val.length() + 2;
                    changed = true;
                } else {
                    pos += name.length();
                }
            }
        }
    }
    return pattern;
}

class RegexParser {
    string p;
    size_t pos;
    NFA* alloc;

    char peek() { return pos < p.length() ? p[pos] : 0; }
    void consume() { pos++; }

    struct NFAFrag { NFAState* start; NFAState* end; };

    NFAFrag parse_union() {
        NFAFrag f = parse_concat();
        while (peek() == '|') {
            consume();
            NFAFrag right = parse_concat();
            NFAState* s = alloc->new_state();
            NFAState* e = alloc->new_state();
            s->add_transition(EPSILON, f.start->id);
            s->add_transition(EPSILON, right.start->id);
            f.end->add_transition(EPSILON, e->id);
            right.end->add_transition(EPSILON, e->id);
            f = {s, e};
        }
        return f;
    }

    NFAFrag parse_concat() {
        NFAFrag f = parse_closure();
        while (peek() && peek() != '|' && peek() != ')') {
            NFAFrag right = parse_closure();
            f.end->add_transition(EPSILON, right.start->id);
            f = {f.start, right.end};
        }
        return f;
    }

    NFAFrag parse_closure() {
        NFAFrag f = parse_atom();
        char c = peek();
        if (c == '*' || c == '+' || c == '?') {
            consume();
            NFAState* s = alloc->new_state();
            NFAState* e = alloc->new_state();
            s->add_transition(EPSILON, f.start->id);
            f.end->add_transition(EPSILON, e->id);
            if (c == '*' || c == '+') {
                f.end->add_transition(EPSILON, f.start->id);
            }
            if (c == '*' || c == '?') {
                s->add_transition(EPSILON, e->id);
            }
            f = {s, e};
        }
        return f;
    }

    NFAFrag parse_atom() {
        if (peek() == '(') {
            consume();
            NFAFrag f = parse_union();
            if (peek() == ')') consume();
            return f;
        } else if (peek() == '[') {
            consume();
            set<int> chars;
            bool negate = false;
            if (peek() == '^') { negate = true; consume(); }

            while (peek() && peek() != ']') {
                int c1 = parse_char();
                if (peek() == '-' && pos + 1 < p.length() && p[pos + 1] != ']') {
                    consume();
                    int c2 = parse_char();
                    for (int i = c1; i <= c2; i++) chars.insert(i);
                } else {
                    chars.insert(c1);
                }
            }
            if (peek() == ']') consume();

            if (negate) {
                set<int> all;
                for (int i = 1; i < 256; i++) {
                    if (chars.find(i) == chars.end()) all.insert(i);
                }
                chars = all;
            }

            NFAState* s = alloc->new_state();
            NFAState* e = alloc->new_state();
            for (int c : chars) s->add_transition(c, e->id);
            return {s, e};
        } else if (peek() == '~') {
            consume();
            int c = parse_char();
            NFAState* s = alloc->new_state();
            NFAState* e = alloc->new_state();
            for (int i = 1; i < 256; i++) {
                if (i != c && i != '\n' && i != '\r') s->add_transition(i, e->id); // usually ~ means not equal to char. if eoln, maybe everything else
            }
            return {s, e};
        } else {
            int c = parse_char();
            NFAState* s = alloc->new_state();
            NFAState* e = alloc->new_state();
            s->add_transition(c, e->id);
            return {s, e};
        }
    }

    int parse_char() {
        if (peek() == '\\') {
            consume();
            char c = peek();
            consume();
            if (c == 'n') return '\n';
            if (c == 't') return '\t';
            if (c == 'r') return '\r';
            // for \+, \*, etc. it returns the literal char exactly
            return c;
        } else {
            char c = peek();
            consume();
            return c;
        }
    }

public:
    RegexParser(string regex, NFA* nfa) : p(regex), pos(0), alloc(nfa) {}
    NFAFrag parse() {
        return parse_union();
    }
};

NFAState* build_branch(NFA* alloc, const string& pattern, int token_id, const string& token_name) {
    RegexParser p(pattern, alloc);
    auto frag = p.parse();
    frag.end->is_accept = true;
    frag.end->token_id = token_id;
    frag.end->token_name = token_name;
    return frag.start;
}

LexerDef parse_lexer_definitions(const string& text) {
    LexerDef def;
    def.global_nfa = new NFA();
    
    NFAState* global_start = def.global_nfa->new_state();
    def.global_nfa->start_state = global_start->id;

    macros.clear();
    macros["eoln"] = "\\n"; 

    stringstream in(text); // read from text directly instead of file
    string line;

    while (getline(in, line)) {
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == string::npos) continue;

        string name = line.substr(0, eq);
        string pattern = line.substr(eq + 1);

        trim(name); trim(pattern);

        string base;
        string num_str;
        string suffix;
        bool has_num = false;

        for (char c : name) {
            if (isdigit(c)) { num_str += c; has_num = true; }
            else if (has_num) { suffix += c; }
            else { base += c; }
        }

        if (!has_num) {
            macros[name] = pattern;
        } else {
            int token_id = stoi(num_str);
            if (base.find("keyword") != string::npos) {
                vector<string> parts = split_top_level(pattern);
                for (string k : parts) {
                    trim(k);
                    def.keywords[k] = token_id++;
                }
            } else if (suffix == "B") {
                vector<string> parts = split_top_level(pattern);
                for (string k : parts) {
                    trim(k);
                    string exp_k = expand_macros(k);
                    NFAState* s = build_branch(def.global_nfa, exp_k, token_id, base + num_str + "_" + to_string(token_id));
                    global_start->add_transition(EPSILON, s->id);
                    token_id++;
                }
            } else {
                string exp_k = expand_macros(pattern);
                NFAState* s = build_branch(def.global_nfa, exp_k, token_id, name);
                global_start->add_transition(EPSILON, s->id);
            }
        }
    }
    return def;
}
