/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        lalr_automata.cpp
 * @brief       用于实现 LALR(1) 自动机构造和解析表生成的模块，核心功能是从文法定义构建 LR(1) 项集族，合并成 LALR(1) 状态，并生成对应的 ACTION 和 GOTO 表
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
#include "lalr_automata.h"
#include <queue>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

LALRAutomata::LALRAutomata(Grammar* g) : grammar(g), conflicts_exist(false) {}

LALRAutomata::~LALRAutomata() {}

void LALRAutomata::generate() {
    build_lr1();
    build_lalr();
    build_tables();
}

// 闭包函数，计算给定项集的闭包，使用队列进行迭代处理，直到没有新的项可以加入为止
std::set<LRItem> LALRAutomata::closure(const std::set<LRItem>& items) const {
    std::set<LRItem> J = items;
    std::queue<LRItem> q;
    for (const auto& item : items) {
        q.push(item);
    }

    while (!q.empty()) {
        LRItem item = q.front();
        q.pop();

        const Production& prod = grammar->productions[item.prod_id];
        
        // If dot is at the end, or the symbol after dot is epsilon/terminal
        if (item.dot_pos >= (int)prod.rhs.size()) continue;
        if (prod.rhs.size() == 1 && prod.rhs[0] == EPSILON_ID) continue; // effectively at end

        int symbol_after_dot = prod.rhs[item.dot_pos];
        if (!grammar->is_non_terminal(symbol_after_dot)) continue;

        std::vector<int> beta;
        for (int i = item.dot_pos + 1; i < (int)prod.rhs.size(); ++i) {
            beta.push_back(prod.rhs[i]);
        }
        beta.push_back(item.lookahead);

        std::set<int> first_beta = grammar->compute_first_of_sequence(beta);

        for (const auto& p : grammar->productions) {
            if (p.lhs == symbol_after_dot) {
                for (int b : first_beta) {
                    if (b == EPSILON_ID) continue; // @ is not a valid lookahead
                    LRItem new_item{p.id, 0, b};
                    if (J.insert(new_item).second) {
                        // Was inserted
                        q.push(new_item);
                    }
                }
            }
        }
    }
    return J;
}


// GOTO 函数，计算从给定项集出发，在输入符号 symbol 下的下一个项集，首先找到所有项中点后符号为 symbol 的项，然后将这些项的点向右移动一位，最后对结果进行闭包运算得到新的项集
std::set<LRItem> LALRAutomata::goto_state(const std::set<LRItem>& items, int symbol) const {
    std::set<LRItem> next_items;
    for (const auto& item : items) {
        const Production& prod = grammar->productions[item.prod_id];
        if (item.dot_pos < (int)prod.rhs.size() && prod.rhs[0] != EPSILON_ID) {
            if (prod.rhs[item.dot_pos] == symbol) {
                next_items.insert({item.prod_id, item.dot_pos + 1, item.lookahead});
            }
        }
    }
    return closure(next_items);
}


// 构建 LR(1) 自动机，使用标准的 LR(1) 项集构造算法，生成所有 LR(1) 状态和转移
void LALRAutomata::build_lr1() {
    LRItem start_item{0, 0, END_MARKER_ID}; // Augmented start production is always ID 0
    std::set<LRItem> initial_items = closure({start_item});

    LRState initial_state;
    initial_state.id = 0;
    initial_state.items = initial_items;

    lr1_states.push_back(initial_state);
    lr1_state_map[initial_items] = 0;

    int current_id = 0;
    while (current_id < (int)lr1_states.size()) {
        std::map<int, std::set<LRItem>> next_states;

        // Find all symbols that can follow dot
        for (const auto& item : lr1_states[current_id].items) {
            const Production& prod = grammar->productions[item.prod_id];
            if (item.dot_pos < (int)prod.rhs.size() && prod.rhs[0] != EPSILON_ID) {
                int sym = prod.rhs[item.dot_pos];
                if (next_states.find(sym) == next_states.end()) {
                    next_states[sym] = goto_state(lr1_states[current_id].items, sym);
                }
            }
        }

        for (const auto& kv : next_states) {
            int sym = kv.first;
            const std::set<LRItem>& nstate_items = kv.second;
            if (nstate_items.empty()) continue;

            auto it = lr1_state_map.find(nstate_items);
            if (it == lr1_state_map.end()) {
                // New state
                int new_id = lr1_states.size();
                LRState nstate;
                nstate.id = new_id;
                nstate.items = nstate_items;
                lr1_states.push_back(nstate);
                lr1_state_map[nstate_items] = new_id;
                
                lr1_states[current_id].transitions[sym] = new_id;
            } else {
                lr1_states[current_id].transitions[sym] = it->second;
            }
        }

        current_id++;
    }
}

std::set<LALRAutomata::ItemCore> LALRAutomata::get_core(const std::set<LRItem>& items) const {
    std::set<ItemCore> core;
    for (const auto& item : items) {
        core.insert({item.prod_id, item.dot_pos});
    }
    return core;
}


// 构建 LALR(1) 自动机，通过合并具有相同核心的 LR(1) 状态来形成 LALR(1) 状态，合并时会将它们的项集合并（即合并展望符），同时重映射状态转移以保持正确的 DFA 结构
void LALRAutomata::build_lalr() {
    std::map<std::set<ItemCore>, int> core_to_lalr_id;
    std::vector<int> lr1_to_lalr(lr1_states.size());

    // Group LR(1) states by core
    for (size_t i = 0; i < lr1_states.size(); ++i) {
        std::set<ItemCore> core = get_core(lr1_states[i].items);
        auto it = core_to_lalr_id.find(core);
        if (it == core_to_lalr_id.end()) {
            int new_id = lalr_states.size();
            core_to_lalr_id[core] = new_id;
            
            LRState nstate;
            nstate.id = new_id;
            nstate.items = lr1_states[i].items;
            lalr_states.push_back(nstate);
            lr1_to_lalr[i] = new_id;
        } else {
            int lalr_id = it->second;
            lr1_to_lalr[i] = lalr_id;
            // Merge items (which effectively merges lookaheads)
            for (const auto& item : lr1_states[i].items) {
                lalr_states[lalr_id].items.insert(item);
            }
        }
    }

    // Remap transitions
    for (size_t i = 0; i < lr1_states.size(); ++i) {
        int lalr_id = lr1_to_lalr[i];
        for (const auto& kv : lr1_states[i].transitions) {
            int sym = kv.first;
            int next_lr1 = kv.second;
            int next_lalr = lr1_to_lalr[next_lr1];
            lalr_states[lalr_id].transitions[sym] = next_lalr;
        }
    }
}


// 构建 LALR(1) 解析表，处理 Shift-Reduce 和 Reduce-Reduce 冲突，记录是否存在冲突以供后续分析
void LALRAutomata::build_tables() {
    action_table.resize(lalr_states.size());
    goto_table.resize(lalr_states.size());

    for (const auto& state : lalr_states) {
        // Shift and Goto actions
        for (const auto& kv : state.transitions) {
            int sym = kv.first;
            int next_state = kv.second;
            if (grammar->is_non_terminal(sym)) {
                goto_table[state.id][sym] = next_state;
            } else {
                Action a;
                a.type = ACTION_SHIFT;
                a.target = next_state;
                if (action_table[state.id].find(sym) != action_table[state.id].end()) {
                    if (action_table[state.id][sym].type != ACTION_SHIFT || action_table[state.id][sym].target != next_state) {
                        conflicts_exist = true; // Shift-Shift conflict? Rare
                    }
                }
                action_table[state.id][sym] = a;
            }
        }

        // Reduce actions
        for (const auto& item : state.items) {
            const Production& prod = grammar->productions[item.prod_id];
            
            bool is_at_end = (item.dot_pos >= (int)prod.rhs.size());
            if (prod.rhs.size() == 1 && prod.rhs[0] == EPSILON_ID) {
                is_at_end = true; // Dot is effectively at end for A -> @
            }

            if (is_at_end) {
                if (item.prod_id == 0) {
                    // Augmented start production
                    if (item.lookahead == END_MARKER_ID) {
                        Action a;
                        a.type = ACTION_ACCEPT;
                        a.target = 0;
                        action_table[state.id][END_MARKER_ID] = a;
                    }
                } else {
                    Action a;
                    a.type = ACTION_REDUCE;
                    a.target = item.prod_id;

                    auto it = action_table[state.id].find(item.lookahead);
                    if (it != action_table[state.id].end()) {
                        // Conflict
                        if (it->second.type == ACTION_SHIFT) {
                            // Shift-Reduce conflict. We'll favor shift for minimal conflict handling.
                            conflicts_exist = true;
                            // Precedence can be applied here, or just keep shift.
                        } else if (it->second.type == ACTION_REDUCE) {
                            // Reduce-Reduce conflict
                            conflicts_exist = true;
                            if (item.prod_id < it->second.target) { // favor earlier rule
                                action_table[state.id][item.lookahead] = a;
                            }
                        }
                    } else {
                        action_table[state.id][item.lookahead] = a;
                    }
                }
            }
        }
    }
}
/* 
void LALRAutomata::print_lr1_states() const {
    std::cout << "\n============= LR(1) Automata =============\n";
    for (const auto& state : lr1_states) {
        std::cout << "State " << state.id << ":\n";
        for (const auto& item : state.items) {
            const auto& prod = grammar->productions[item.prod_id];
            std::cout << "  " << grammar->get_symbol_str(prod.lhs) << " -> ";
            for (int i = 0; i < (int)prod.rhs.size(); ++i) {
                if (i == item.dot_pos) std::cout << ". ";
                std::cout << ((prod.rhs[i] == EPSILON_ID) ? "@" : grammar->get_symbol_str(prod.rhs[i])) << " ";
            }
            if (item.dot_pos == (int)prod.rhs.size()) std::cout << ". ";
            std::cout << ", " << grammar->get_symbol_str(item.lookahead) << "\n";
        }
        for (const auto& kv : state.transitions) {
            std::cout << "  goto(" << grammar->get_symbol_str(kv.first) << ") -> State " << kv.second << "\n";
        }
    }
}
*/
// 打印 LR(1) 状态表，格式化输出，使得每个状态的项和转移能够并排显示，方便对比和分析 
void LALRAutomata::print_lr1_states() const {
    std::cout << "\n================================================== LR(1) Automata DFA States Table ==================================================\n";
    std::cout << std::left 
              << std::setw(10) << "State" 
              << std::setw(80) << "LR(1) Items (Production Rules & Lookaheads)" 
              << "Transitions (Symbol -> Next State)\n";
    std::cout << std::string(150, '-') << "\n";

    for (const auto& state : lr1_states) {
        bool first_row_of_state = true;
        size_t item_idx = 0;
        
        // 收集当前状态的所有转移边字符串，方便和 Items 并排对齐输出
        std::vector<std::string> trans_strs;
        for (const auto& kv : state.transitions) {
            trans_strs.push_back(grammar->get_symbol_str(kv.first) + " -> S" + std::to_string(kv.second));
        }

        auto items_vector = std::vector<LRItem>(state.items.begin(), state.items.end());
        size_t max_rows = std::max(items_vector.size(), trans_strs.size());

        for (size_t i = 0; i < max_rows; ++i) {
            // 1. 打印状态 ID (只在当前状态的第一行打印，其余留空)
            if (first_row_of_state) {
                std::cout << std::left << std::setw(10) << ("I" + std::to_string(state.id));
                first_row_of_state = false;
            } else {
                std::cout << std::left << std::setw(10) << " ";
            }

            // 2. 打印 LR(1) 产生式项
            if (i < items_vector.size()) {
                const auto& item = items_vector[i];
                const auto& prod = grammar->productions[item.prod_id];
                
                std::stringstream item_ss;
                item_ss << grammar->get_symbol_str(prod.lhs) << " -> ";
                for (int j = 0; j < (int)prod.rhs.size(); ++j) {
                    if (j == item.dot_pos) item_ss << ". ";
                    item_ss << ((prod.rhs[j] == EPSILON_ID) ? "@" : grammar->get_symbol_str(prod.rhs[j])) << " ";
                }
                if (item.dot_pos == (int)prod.rhs.size()) item_ss << ". ";
                item_ss << ", [" << grammar->get_symbol_str(item.lookahead) << "]";
                
                std::cout << std::left << std::setw(80) << item_ss.str();
            } else {
                std::cout << std::left << std::setw(80) << " ";
            }

            // 3. 打印 状态转移
            if (i < trans_strs.size()) {
                std::cout << trans_strs[i];
            }
            
            std::cout << "\n";
        }
        std::cout << std::string(150, '-') << "\n"; // 每个状态间的分隔线
    }
}


/*
void LALRAutomata::print_lalr_states() const {
    std::cout << "\n============= LALR(1) Automata =============\n";
    for (const auto& state : lalr_states) {
        std::cout << "State " << state.id << ":\n";
        for (const auto& item : state.items) {
            const auto& prod = grammar->productions[item.prod_id];
            std::cout << "  " << grammar->get_symbol_str(prod.lhs) << " -> ";
            for (int i = 0; i < (int)prod.rhs.size(); ++i) {
                if (i == item.dot_pos) std::cout << ". ";
                std::cout << ((prod.rhs[i] == EPSILON_ID) ? "@" : grammar->get_symbol_str(prod.rhs[i])) << " ";
            }
            if (item.dot_pos == (int)prod.rhs.size()) std::cout << ". ";
            std::cout << ", " << grammar->get_symbol_str(item.lookahead) << "\n";
        }
        for (const auto& kv : state.transitions) {
            std::cout << "  goto(" << grammar->get_symbol_str(kv.first) << ") -> State " << kv.second << "\n";
        }
    }
}
*/


// 打印 LALR(1) 状态表，合并同心项的展望符，并且格式化输出，使得每个状态的项和转移能够并排显示，方便对比和分析
void LALRAutomata::print_lalr_states() const {
    std::cout << "\n================================================= LALR(1) Automata DFA States Table =================================================\n";
    std::cout << std::left 
              << std::setw(10) << "State" 
              << std::setw(80) << "LALR(1) Merged Items (Core & Lookaheads)" 
              << "Transitions (Symbol -> Next State)\n";
    std::cout << std::string(150, '-') << "\n";

    for (const auto& state : lalr_states) {
        // 合并同心项的展望符
        std::map<std::pair<int, int>, std::set<int>> core_to_lookaheads;
        for (const auto& item : state.items) {
            core_to_lookaheads[{item.prod_id, item.dot_pos}].insert(item.lookahead);
        }

        std::vector<std::string> item_strs;
        for (const auto& kv : core_to_lookaheads) {
            int prod_id = kv.first.first;
            int dot_pos = kv.first.second;
            const auto& lookaheads = kv.second;

            const auto& prod = grammar->productions[prod_id];
            std::stringstream item_ss;
            item_ss << grammar->get_symbol_str(prod.lhs) << " -> ";
            for (int j = 0; j < (int)prod.rhs.size(); ++j) {
                if (j == dot_pos) item_ss << ". ";
                item_ss << ((prod.rhs[j] == EPSILON_ID) ? "@" : grammar->get_symbol_str(prod.rhs[j])) << " ";
            }
            if (dot_pos == (int)prod.rhs.size()) item_ss << ". ";
            
            item_ss << ", [ ";
            for (int la : lookaheads) {
                item_ss << grammar->get_symbol_str(la) << " ";
            }
            item_ss << "]";
            item_strs.push_back(item_ss.str());
        }

        // 收集转移
        std::vector<std::string> trans_strs;
        for (const auto& kv : state.transitions) {
            trans_strs.push_back(grammar->get_symbol_str(kv.first) + " -> S" + std::to_string(kv.second));
        }

        size_t max_rows = std::max(item_strs.size(), trans_strs.size());
        bool first_row_of_state = true;

        for (size_t i = 0; i < max_rows; ++i) {
            if (first_row_of_state) {
                std::cout << std::left << std::setw(10) << ("I" + std::to_string(state.id));
                first_row_of_state = false;
            } else {
                std::cout << std::left << std::setw(10) << " ";
            }

            if (i < item_strs.size()) {
                std::cout << std::left << std::setw(80) << item_strs[i];
            } else {
                std::cout << std::left << std::setw(80) << " ";
            }

            if (i < trans_strs.size()) {
                std::cout << trans_strs[i];
            }
            std::cout << "\n";
        }
        std::cout << std::string(150, '-') << "\n";
    }
}


// 打印 LALR(1) 解析表，格式化输出，使得每个状态的动作和转移能够并排显示，方便对比和分析，同时如果存在冲突会有明显的提示
void LALRAutomata::print_parsing_table() const {
    std::cout << "\n============= LALR(1) Parsing Table =============\n";
    if (conflicts_exist) {
        std::cout << "WARNING: Conflicts exist in parsing table!\n";
    }
    for (size_t i = 0; i < action_table.size(); ++i) {
        std::cout << "State " << i << ":\n";
        for (const auto& kv : action_table[i]) {
            std::cout << "  Action[" << grammar->get_symbol_str(kv.first) << "] = ";
            if (kv.second.type == ACTION_SHIFT) std::cout << "S" << kv.second.target << "\n";
            else if (kv.second.type == ACTION_REDUCE) std::cout << "R" << kv.second.target << " (" << grammar->get_symbol_str(grammar->productions[kv.second.target].lhs) << " -> ...)\n";
            else if (kv.second.type == ACTION_ACCEPT) std::cout << "ACC\n";
            else std::cout << "ERR\n";
        }
        for (const auto& kv : goto_table[i]) {
            std::cout << "  Goto[" << grammar->get_symbol_str(kv.first) << "] = " << kv.second << "\n";
        }
    }
}
