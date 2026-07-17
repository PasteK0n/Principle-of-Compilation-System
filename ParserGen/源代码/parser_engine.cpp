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
#include <functional>
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

static void collect_statements(SyntaxTreeNode* node, std::vector<SyntaxTreeNode*>& list, const std::function<SyntaxTreeNode*(SyntaxTreeNode*)>& convert_fn) {
    if (!node) return;
    if (node->name == "stmt-sequence" || node->name == "program" || node->name == "statement") {
        for (auto child : node->children) {
            collect_statements(child, list, convert_fn);
        }
    } else if (node->name == ";") {
        // Skip semicolon node in AST
        return;
    } else {
        SyntaxTreeNode* ast = convert_fn(node);
        if (ast) {
            list.push_back(ast);
        }
    }
}

static SyntaxTreeNode* convert_cst_to_ast(SyntaxTreeNode* node) {
    if (!node) return nullptr;

    // Standard recursive AST converter for TINY grammar
    if (node->name == "program") {
        SyntaxTreeNode* ast_root = new SyntaxTreeNode(node->id, "start");
        std::vector<SyntaxTreeNode*> stmts;
        collect_statements(node, stmts, convert_cst_to_ast);
        ast_root->children = stmts;
        return ast_root;
    }
    
    if (node->name == "stmt-sequence" || node->name == "statement") {
        std::vector<SyntaxTreeNode*> stmts;
        collect_statements(node, stmts, convert_cst_to_ast);
        if (stmts.empty()) return nullptr;
        if (stmts.size() == 1) return stmts[0];
        SyntaxTreeNode* seq = new SyntaxTreeNode(node->id, "sequence");
        seq->children = stmts;
        return seq;
    }

    if (node->name == "if-stmt") {
        SyntaxTreeNode* if_node = new SyntaxTreeNode(node->id, "if");
        if (node->children.size() > 1) {
            if_node->children.push_back(convert_cst_to_ast(node->children[1]));
        }
        if (node->children.size() > 3) {
            std::vector<SyntaxTreeNode*> then_stmts;
            collect_statements(node->children[3], then_stmts, convert_cst_to_ast);
            for (auto s : then_stmts) {
                if_node->children.push_back(s);
            }
        }
        if (node->children.size() > 5 && node->children[4]->name == "else") {
            SyntaxTreeNode* else_node = new SyntaxTreeNode(node->children[4]->id, "else");
            std::vector<SyntaxTreeNode*> else_stmts;
            collect_statements(node->children[5], else_stmts, convert_cst_to_ast);
            else_node->children = else_stmts;
            if_node->children.push_back(else_node);
        }
        return if_node;
    }

    if (node->name == "repeat-stmt") {
        SyntaxTreeNode* repeat_node = new SyntaxTreeNode(node->id, "repeat");
        if (node->children.size() > 1) {
            std::vector<SyntaxTreeNode*> body_stmts;
            collect_statements(node->children[1], body_stmts, convert_cst_to_ast);
            for (auto s : body_stmts) {
                repeat_node->children.push_back(s);
            }
        }
        if (node->children.size() > 3) {
            repeat_node->children.push_back(convert_cst_to_ast(node->children[3]));
        }
        return repeat_node;
    }

    if (node->name == "assign-stmt") {
        SyntaxTreeNode* assign_node = new SyntaxTreeNode(node->id, "assign");
        if (node->children.size() > 0) {
            assign_node->children.push_back(convert_cst_to_ast(node->children[0]));
        }
        if (node->children.size() > 2) {
            assign_node->children.push_back(convert_cst_to_ast(node->children[2]));
        }
        return assign_node;
    }

    if (node->name == "read-stmt") {
        SyntaxTreeNode* read_node = new SyntaxTreeNode(node->id, "read");
        if (node->children.size() > 1) {
            read_node->children.push_back(convert_cst_to_ast(node->children[1]));
        }
        return read_node;
    }

    if (node->name == "write-stmt") {
        SyntaxTreeNode* write_node = new SyntaxTreeNode(node->id, "write");
        if (node->children.size() > 1) {
            write_node->children.push_back(convert_cst_to_ast(node->children[1]));
        }
        return write_node;
    }

    if (node->name == "exp") {
        if (node->children.size() == 3) {
            std::string op = "=";
            if (node->children[1]->children.size() > 0) {
                op = node->children[1]->children[0]->name;
            }
            SyntaxTreeNode* op_node = new SyntaxTreeNode(node->id, op);
            op_node->children.push_back(convert_cst_to_ast(node->children[0]));
            op_node->children.push_back(convert_cst_to_ast(node->children[2]));
            return op_node;
        } else if (node->children.size() == 1) {
            return convert_cst_to_ast(node->children[0]);
        }
    }

    if (node->name == "simple-exp") {
        if (node->children.size() == 3) {
            std::string op = "+";
            if (node->children[1]->children.size() > 0) {
                op = node->children[1]->children[0]->name;
            }
            SyntaxTreeNode* op_node = new SyntaxTreeNode(node->id, "op(" + op + ")");
            op_node->children.push_back(convert_cst_to_ast(node->children[0]));
            op_node->children.push_back(convert_cst_to_ast(node->children[2]));
            return op_node;
        } else if (node->children.size() == 1) {
            return convert_cst_to_ast(node->children[0]);
        }
    }

    if (node->name == "term") {
        if (node->children.size() == 3) {
            std::string op = "*";
            if (node->children[1]->children.size() > 0) {
                op = node->children[1]->children[0]->name;
            }
            SyntaxTreeNode* op_node = new SyntaxTreeNode(node->id, "op(" + op + ")");
            op_node->children.push_back(convert_cst_to_ast(node->children[0]));
            op_node->children.push_back(convert_cst_to_ast(node->children[2]));
            return op_node;
        } else if (node->children.size() == 1) {
            return convert_cst_to_ast(node->children[0]);
        }
    }

    if (node->name == "power") {
        if (node->children.size() == 1) {
            return convert_cst_to_ast(node->children[0]);
        }
    }

    if (node->name == "factor") {
        if (node->children.size() == 3) {
            return convert_cst_to_ast(node->children[1]);
        } else if (node->children.size() == 1) {
            return convert_cst_to_ast(node->children[0]);
        }
    }

    if (node->name == "identifier") {
        return new SyntaxTreeNode(node->id, "id(" + node->lexeme + ")");
    }

    if (node->name == "number") {
        return new SyntaxTreeNode(node->id, "const(" + node->lexeme + ")");
    }

    if (node->children.size() == 1) {
        return convert_cst_to_ast(node->children[0]);
    }

    SyntaxTreeNode* copy_node = new SyntaxTreeNode(node->id, node->name, node->lexeme);
    for (auto child : node->children) {
        SyntaxTreeNode* simplified_child = convert_cst_to_ast(child);
        if (simplified_child) {
            copy_node->children.push_back(simplified_child);
        }
    }
    return copy_node;
}

SyntaxTreeNode* ParserEngine::get_ast_tree() const {
    return convert_cst_to_ast(syntax_tree_root);
}

std::string ParserEngine::get_syntax_tree_string() const {
    if (!syntax_tree_root) return "No syntax tree generated.";
    
    SyntaxTreeNode* ast_root = get_ast_tree();
    if (!ast_root) return "Failed to generate AST.";
    
    std::stringstream ss;
    
    std::function<void(SyntaxTreeNode*, std::string, bool, bool)> print_node_fn = 
        [&](SyntaxTreeNode* node, std::string prefix, bool is_last, bool is_root) {
            if (!node) return;
            bool has_children = node->children.size() > 0;
            std::string name_label = has_children ? ("[-] " + node->name) : node->name;
            
            if (is_root) {
                ss << name_label << "\n";
                prefix = "";
            } else {
                std::string connector = is_last ? "└─ " : "├─ ";
                ss << prefix << connector << name_label << "\n";
                prefix += is_last ? "   " : "│  ";
            }
            
            for (size_t i = 0; i < node->children.size(); ++i) {
                print_node_fn(node->children[i], prefix, i == node->children.size() - 1, false);
            }
        };
        
    print_node_fn(ast_root, "", true, true);
    
    delete ast_root;
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

struct TACInstr {
    std::string label;  // Optional label before command
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
};

class CodeGenerator {
private:
    int temp_idx;
    int label_idx;
    std::vector<TACInstr> instrs;

    std::string new_temp() {
        return "t" + std::to_string(++temp_idx);
    }

    std::string new_label() {
        return "L" + std::to_string(++label_idx);
    }

public:
    CodeGenerator() : temp_idx(0), label_idx(0) {}

    // Generate code recursively starting from AST node
    std::string generate_value(SyntaxTreeNode* node) {
        if (!node) return "";

        // If it's a leaf node representing identifier or constant
        if (node->children.empty()) {
            std::string name = node->name;
            if (name.rfind("id(", 0) == 0 && name.back() == ')') {
                return name.substr(3, name.length() - 4);
            }
            if (name.rfind("const(", 0) == 0 && name.back() == ')') {
                return name.substr(6, name.length() - 7);
            }
            return name;
        }

        // Handle operator node op(...) or relation operator like <, =, etc.
        if (node->name.rfind("op(", 0) == 0 || node->name == "<" || node->name == "=" || node->name == ">" || node->name == "<=" || node->name == ">=" || node->name == "<>") {
            std::string op_char = node->name;
            if (op_char.rfind("op(", 0) == 0) {
                // Extract op from "op(+)"
                op_char = op_char.substr(3, op_char.length() - 4);
            }
            if (op_char == "=") {
                op_char = "=="; // use == for equality comparison to distinguish from assignment
            }
            std::string val1 = generate_value(node->children[0]);
            std::string val2 = generate_value(node->children[1]);
            std::string temp = new_temp();
            
            TACInstr inst;
            inst.op = op_char;
            inst.arg1 = val1;
            inst.arg2 = val2;
            inst.result = temp;
            instrs.push_back(inst);
            return temp;
        }

        // Default fallback for expression subtrees
        if (node->children.size() == 1) {
            return generate_value(node->children[0]);
        }
        return "";
    }

    void generate_stmt(SyntaxTreeNode* node) {
        if (!node) return;

        if (node->name == "start" || node->name == "sequence") {
            for (auto child : node->children) {
                generate_stmt(child);
            }
        }
        else if (node->name == "read") {
            TACInstr inst;
            inst.op = "read";
            inst.result = generate_value(node->children[0]);
            instrs.push_back(inst);
        }
        else if (node->name == "write") {
            std::string val = generate_value(node->children[0]);
            TACInstr inst;
            inst.op = "write";
            inst.result = val;
            instrs.push_back(inst);
        }
        else if (node->name == "assign") {
            std::string dest = generate_value(node->children[0]);
            std::string src = generate_value(node->children[1]);
            TACInstr inst;
            inst.op = "=";
            inst.arg1 = src;
            inst.result = dest;
            instrs.push_back(inst);
        }
        else if (node->name == "if") {
            // Children: index 0 is cond, followed by then statements, and optionally "else" statement node
            std::string cond = generate_value(node->children[0]);
            std::string label_else = new_label();
            std::string label_end = new_label();

            // Check if there is an else node (the last child)
            bool has_else = false;
            size_t then_end_idx = node->children.size();
            if (node->children.size() > 1 && node->children.back()->name == "else") {
                has_else = true;
                then_end_idx = node->children.size() - 1;
            }

            // if_false cond goto label_else (or label_end if no else)
            TACInstr cond_inst;
            cond_inst.op = "if_false";
            cond_inst.arg1 = cond;
            cond_inst.result = has_else ? label_else : label_end;
            instrs.push_back(cond_inst);

            // Generate then body statements
            for (size_t i = 1; i < then_end_idx; ++i) {
                generate_stmt(node->children[i]);
            }

            if (has_else) {
                // goto label_end
                TACInstr goto_inst;
                goto_inst.op = "goto";
                goto_inst.result = label_end;
                instrs.push_back(goto_inst);

                // label_else:
                TACInstr label_inst;
                label_inst.label = label_else;
                instrs.push_back(label_inst);

                // Generate else body statements
                SyntaxTreeNode* else_node = node->children.back();
                for (auto child : else_node->children) {
                    generate_stmt(child);
                }
            }

            // label_end:
            TACInstr end_label_inst;
            end_label_inst.label = label_end;
            instrs.push_back(end_label_inst);
        }
        else if (node->name == "repeat") {
            std::string label_start = new_label();
            
            // label_start:
            TACInstr start_label_inst;
            start_label_inst.label = label_start;
            instrs.push_back(start_label_inst);

            // Repeat body is all children except the last one (which is the until condition)
            size_t body_end_idx = node->children.size() - 1;
            for (size_t i = 0; i < body_end_idx; ++i) {
                generate_stmt(node->children[i]);
            }

            // Until condition is the last child
            std::string cond = generate_value(node->children.back());

            // repeat until cond is true (so jump back if false)
            TACInstr loop_inst;
            loop_inst.op = "if_false";
            loop_inst.arg1 = cond;
            loop_inst.result = label_start;
            instrs.push_back(loop_inst);
        }
    }

    std::string get_tac_string() const {
        std::stringstream ss;
        for (const auto& inst : instrs) {
            if (!inst.label.empty()) {
                ss << inst.label << ":\n";
            }
            if (!inst.op.empty()) {
                ss << "    ";
                if (inst.op == "read" || inst.op == "write") {
                    ss << inst.op << " " << inst.result << "\n";
                }
                else if (inst.op == "goto") {
                    ss << "goto " << inst.result << "\n";
                }
                else if (inst.op == "if_false") {
                    ss << "if_false " << inst.arg1 << " goto " << inst.result << "\n";
                }
                else if (inst.op == "=" && inst.arg2.empty()) {
                    ss << inst.result << " = " << inst.arg1 << "\n";
                }
                else {
                    ss << inst.result << " = " << inst.arg1 << " " << inst.op << " " << inst.arg2 << "\n";
                }
            }
        }
        return ss.str();
    }

    std::string get_quadruples_string() const {
        std::stringstream ss;
        int index = 0;
        for (const auto& inst : instrs) {
            if (!inst.op.empty()) {
                std::string op = inst.op;
                std::string arg1 = inst.arg1.empty() ? "-" : inst.arg1;
                std::string arg2 = inst.arg2.empty() ? "-" : inst.arg2;
                std::string result = inst.result.empty() ? "-" : inst.result;
                
                ss << std::left << std::setw(4) << index++ << ": (" 
                   << std::left << std::setw(10) << op << ", "
                   << std::left << std::setw(12) << arg1 << ", "
                   << std::left << std::setw(12) << arg2 << ", "
                   << std::left << std::setw(12) << result << ")\n";
            } else if (!inst.label.empty()) {
                // Quadruple representing label definition
                ss << std::left << std::setw(4) << index++ << ": (" 
                   << std::left << std::setw(10) << "label" << ", "
                   << std::left << std::setw(12) << "-" << ", "
                   << std::left << std::setw(12) << "-" << ", "
                   << std::left << std::setw(12) << inst.label << ")\n";
            }
        }
        return ss.str();
    }
};

std::string ParserEngine::get_intermediate_code_string() const {
    if (!syntax_tree_root) return "No intermediate code generated.";
    
    SyntaxTreeNode* ast_root = get_ast_tree();
    if (!ast_root) return "Failed to generate AST for intermediate code.";

    CodeGenerator gen;
    gen.generate_stmt(ast_root);
    delete ast_root;

    std::stringstream ss;
    ss << "=== THREE-ADDRESS CODE (TAC) ===\n";
    ss << gen.get_tac_string() << "\n";
    ss << "=== QUADRUPLES ===\n";
    ss << gen.get_quadruples_string();

    return ss.str();
}
