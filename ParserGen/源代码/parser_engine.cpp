/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        parser_engine.cpp
 * @brief       用于实现LALR分析器的核心解析引擎，负责执行LR(1)分析过程，构建语法树，并记录详细的解析步骤以供UI展示
 *
 * (3) 当前版本号，作者/修改者，完成日期
 * @version     V1.0.0
 * @author      陈乐豪
 * @date        2026-04-28
 *
 * (4) 版本历史信息 (Revision History)
 * -------------------------------------------------------------------------
 * Version     Date          Author          Description
 * V1.0.0      2026-04-28    陈乐豪      1. 创建文件并初始化基础功能
 ******************************************************************************/
#include "parser_engine.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>

ParserEngine::ParserEngine(Grammar* g, LALRAutomata* aut)
    : grammar(g), automata(aut), syntax_tree_root(nullptr), next_node_id(1) {
    
    // Set standard default mapping for TINY
    token_map[100] = "number";
    token_map[101] = "identifier";
    token_map[103] = "if";
    token_map[104] = "then";
    token_map[105] = "else";
    token_map[106] = "end";
    token_map[107] = "repeat";
    token_map[108] = "until";
    token_map[109] = "read";
    token_map[110] = "write";
    token_map[200] = ":=";
    token_map[201] = "+";
    token_map[202] = "-";
    token_map[203] = "*";
    token_map[204] = "/";
    token_map[205] = "=";
    token_map[206] = "<";
    token_map[207] = "(";
    token_map[208] = ")";
    token_map[209] = ";";
}

ParserEngine::~ParserEngine() {
    clean_tree();
}

void ParserEngine::clean_tree() {
    if (syntax_tree_root) {
        delete syntax_tree_root;
        syntax_tree_root = nullptr;
    }
}

void ParserEngine::set_token_map(const std::map<int, std::string>& mapping) {
    token_map = mapping;
}

void ParserEngine::load_token_map_from_string(const std::string& map_str) {
    token_map.clear();
    std::stringstream ss(map_str);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream line_ss(line);
        int tid;
        std::string sym;
        if (line_ss >> tid >> sym) {
            token_map[tid] = sym;
        }
    }
}

std::string ParserEngine::get_token_map_string() const {
    std::stringstream ss;
    for (auto const& kv : token_map) {
        ss << kv.first << " " << kv.second << "\n";
    }
    return ss.str();
}

std::vector<TokenInstance> ParserEngine::tokenize_lex_file(const std::string& content) {
    std::vector<TokenInstance> tokens;
    std::stringstream ss(content);
    int tid;
    while (ss >> tid) {
        TokenInstance tok;
        tok.token_id = tid;
        
        if (token_map.count(tid)) {
            tok.grammar_symbol = token_map[tid];
        } else {
            // Default fallback if token not mapped
            tok.grammar_symbol = "unknown_" + std::to_string(tid);
        }

        // We know ID/number have lexemes
        if (tok.grammar_symbol == "identifier" || tok.grammar_symbol == "ID" ||
            tok.grammar_symbol == "number" || tok.grammar_symbol == "NUM") {
            std::string lex;
            ss >> lex;
            // Strip quotes if any
            if (!lex.empty() && lex.front() == '"') {
                if (lex.back() == '"') {
                    lex = lex.substr(1, lex.size() - 2);
                } else {
                    std::string rest;
                    while (ss >> rest) {
                        lex += " " + rest;
                        if (rest.back() == '"') {
                            break;
                        }
                    }
                    if (lex.size() >= 2) {
                        lex = lex.substr(1, lex.size() - 2);
                    }
                }
            }
            tok.lexeme = lex;
        } else {
            tok.lexeme = tok.grammar_symbol; // lexeme is the symbol representation
        }
        tokens.push_back(tok);
    }
    
    // Append END MARKER
    TokenInstance end_tok;
    end_tok.token_id = END_MARKER_ID;
    end_tok.lexeme = "$";
    end_tok.grammar_symbol = "$";
    tokens.push_back(end_tok);

    return tokens;
}

bool ParserEngine::parse(const std::string& lex_file_content) {
    clean_tree();
    parse_steps.clear();
    next_node_id = 1;

    std::vector<TokenInstance> input = tokenize_lex_file(lex_file_content);
    if (input.empty()) return false;

    std::stack<int> state_stack;
    std::stack<SyntaxTreeNode*> symbol_stack;
    
    state_stack.push(0);

    size_t ip = 0;
    int step_cnt = 1;

    while (true) {
        int s = state_stack.top();
        TokenInstance t = input[ip];

        // Convert token_id to a grammar symbol ID
        int lookahead_sym_id = grammar->get_symbol_id(t.grammar_symbol, false);
        if (lookahead_sym_id == -1) {
            // End marker check
            if (t.grammar_symbol == "$") {
                lookahead_sym_id = END_MARKER_ID;
            } else {
                // Syntax error: Unknown token in grammar
                ParseStep step;
                step.step_number = step_cnt;
                step.action_desc = "ERROR: Token '" + t.grammar_symbol + "' not in grammar";
                
                // Record stacks for UI
                std::stack<int> temp_states = state_stack;
                while(!temp_states.empty()) { step.state_stack.push_back(temp_states.top()); temp_states.pop(); }
                std::reverse(step.state_stack.begin(), step.state_stack.end());

                std::stack<SyntaxTreeNode*> temp_symbols = symbol_stack;
                while(!temp_symbols.empty()) { step.symbol_stack.push_back(temp_symbols.top()->name); temp_symbols.pop(); }
                std::reverse(step.symbol_stack.begin(), step.symbol_stack.end());

                step.remaining_input.assign(input.begin() + ip, input.end());
                parse_steps.push_back(step);
                return false;
            }
        }

        // Action check
        if (s >= (int)automata->action_table.size() || automata->action_table[s].count(lookahead_sym_id) == 0) {
            // Syntax error
            ParseStep step;
            step.step_number = step_cnt;
            step.action_desc = "ERROR: Unexpected token '" + t.grammar_symbol + "' in state " + std::to_string(s);
            
            std::stack<int> temp_states = state_stack;
            while(!temp_states.empty()) { step.state_stack.push_back(temp_states.top()); temp_states.pop(); }
            std::reverse(step.state_stack.begin(), step.state_stack.end());

            std::stack<SyntaxTreeNode*> temp_symbols = symbol_stack;
            while(!temp_symbols.empty()) { step.symbol_stack.push_back(temp_symbols.top()->name); temp_symbols.pop(); }
            std::reverse(step.symbol_stack.begin(), step.symbol_stack.end());

            step.remaining_input.assign(input.begin() + ip, input.end());
            parse_steps.push_back(step);
            return false;
        }

        Action act = automata->action_table[s][lookahead_sym_id];

        // Step record
        ParseStep step;
        step.step_number = step_cnt++;
        
        // Record stacks
        std::stack<int> temp_states = state_stack;
        while(!temp_states.empty()) { step.state_stack.push_back(temp_states.top()); temp_states.pop(); }
        std::reverse(step.state_stack.begin(), step.state_stack.end());

        std::stack<SyntaxTreeNode*> temp_symbols = symbol_stack;
        while(!temp_symbols.empty()) { step.symbol_stack.push_back(temp_symbols.top()->name); temp_symbols.pop(); }
        std::reverse(step.symbol_stack.begin(), step.symbol_stack.end());

        step.remaining_input.assign(input.begin() + ip, input.end());

        if (act.type == ACTION_SHIFT) {
            step.action_desc = "Shift to state " + std::to_string(act.target);
            parse_steps.push_back(step);

            state_stack.push(act.target);
            symbol_stack.push(new SyntaxTreeNode(next_node_id++, t.grammar_symbol, t.lexeme));
            ip++;
        }
        else if (act.type == ACTION_REDUCE) {
            const Production& prod = grammar->productions[act.target];
            step.action_desc = "Reduce by: " + grammar->get_symbol_str(prod.lhs) + " -> ";
            if (prod.rhs.empty() || (prod.rhs.size() == 1 && prod.rhs[0] == EPSILON_ID)) {
                step.action_desc += "@";
            } else {
                for (int sym : prod.rhs) step.action_desc += grammar->get_symbol_str(sym) + " ";
            }
            parse_steps.push_back(step);

            int pop_cnt = (prod.rhs.size() == 1 && prod.rhs[0] == EPSILON_ID) ? 0 : prod.rhs.size();
            
            std::vector<SyntaxTreeNode*> children;
            for (int i = 0; i < pop_cnt; ++i) {
                state_stack.pop();
                children.push_back(symbol_stack.top());
                symbol_stack.pop();
            }
            std::reverse(children.begin(), children.end());

            SyntaxTreeNode* parent = new SyntaxTreeNode(next_node_id++, grammar->get_symbol_str(prod.lhs));
            if (pop_cnt == 0) {
                // If it's an epsilon reduction, add a leaf node for epsilon
                parent->children.push_back(new SyntaxTreeNode(next_node_id++, "@"));
            } else {
                parent->children = children;
            }

            int s_top = state_stack.top();
            if (automata->goto_table[s_top].count(prod.lhs) == 0) {
                // Goto error
                return false;
            }
            int next_s = automata->goto_table[s_top][prod.lhs];
            state_stack.push(next_s);
            symbol_stack.push(parent);
        }
        else if (act.type == ACTION_ACCEPT) {
            step.action_desc = "Accept";
            parse_steps.push_back(step);
            
            if (!symbol_stack.empty()) {
                syntax_tree_root = symbol_stack.top();
            }
            break;
        }
    }

    return true;
}

std::string ParserEngine::get_syntax_tree_string() const {
    if (!syntax_tree_root) return "No syntax tree generated.";
    std::stringstream ss;
    print_syntax_tree(syntax_tree_root, 0, ss);
    return ss.str();
}

void ParserEngine::print_syntax_tree(SyntaxTreeNode* node, int depth, std::stringstream& ss) const {
    if (!node) return;
    for (int i = 0; i < depth; ++i) {
        ss << "  ";
    }
    ss << "|-- " << node->name;
    if (!node->lexeme.empty() && node->lexeme != node->name) {
        ss << " (" << node->lexeme << ")";
    }
    ss << "\n";
    for (auto child : node->children) {
        print_syntax_tree(child, depth + 1, ss);
    }
}

std::string ParserEngine::get_parse_log() const {
    std::stringstream ss;
    ss << std::left << std::setw(6) << "Step" 
       << std::left << std::setw(40) << "State Stack" 
       << std::left << std::setw(40) << "Symbol Stack" 
       << std::left << std::setw(30) << "Input Buffer" 
       << "Action\n";
    ss << std::string(130, '-') << "\n";

    for (const auto& step : parse_steps) {
        // States string
        std::stringstream states_ss;
        for (size_t i = 0; i < step.state_stack.size(); ++i) {
            states_ss << step.state_stack[i] << (i == step.state_stack.size() - 1 ? "" : " ");
        }

        // Symbols string
        std::stringstream symbols_ss;
        for (size_t i = 0; i < step.symbol_stack.size(); ++i) {
            symbols_ss << step.symbol_stack[i] << (i == step.symbol_stack.size() - 1 ? "" : " ");
        }

        // Input string (first 3 tokens to keep it clean)
        std::stringstream input_ss;
        for (size_t i = 0; i < std::min((size_t)3, step.remaining_input.size()); ++i) {
            input_ss << step.remaining_input[i].lexeme << " ";
        }
        if (step.remaining_input.size() > 3) {
            input_ss << "...";
        }

        ss << std::left << std::setw(6) << step.step_number 
           << std::left << std::setw(40) << states_ss.str() 
           << std::left << std::setw(40) << symbols_ss.str() 
           << std::left << std::setw(30) << input_ss.str() 
           << step.action_desc << "\n";
    }
    return ss.str();
}
