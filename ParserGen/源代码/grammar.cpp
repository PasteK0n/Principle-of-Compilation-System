/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        grammar.cpp
 * @brief       用于实现语法分析器的模块，核心功能是处理文法定义和生产规则
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
#include "grammar.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

Grammar::Grammar() : start_symbol_id(-1) {
    get_symbol_id("@", true); // ID 0
    get_symbol_id("$", true); // ID 1
}

Grammar::~Grammar() {}

int Grammar::get_symbol_id(const std::string& sym, bool create_if_missing) {
    auto it = symbol_to_id.find(sym);
    if (it != symbol_to_id.end()) {
        return it->second;
    }
    if (create_if_missing) {
        int new_id = id_to_symbol.size();
        symbol_to_id[sym] = new_id;
        id_to_symbol.push_back(sym);
        return new_id;
    }
    return -1;
}

std::string Grammar::get_symbol_str(int id) const {
    if (id >= 0 && id < (int)id_to_symbol.size()) {
        return id_to_symbol[id];
    }
    return "";
}

bool Grammar::is_non_terminal(int id) const {
    return non_terminals.count(id) > 0;
}

std::vector<std::string> Grammar::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current_token;
    
    // We need to carefully handle "->" as a single token, or split by it.
    // However, terminals might be "+" or "-" or "->". Let's assume spaces separate tokens mostly,
    // but the user might write "E->E+T". We'll do a simple space-based tokenization first,
    // and if needed, handle "->" replacing it with " -> " before parsing.
    
    std::string processed_line = line;
    // Basic replace "->" with " -> " to ensure it's a separate token
    size_t pos = 0;
    while ((pos = processed_line.find("->", pos)) != std::string::npos) {
        processed_line.replace(pos, 2, " -> ");
        pos += 4;
    }
    // Also replace "|" with " | "
    pos = 0;
    while ((pos = processed_line.find("|", pos)) != std::string::npos) {
        processed_line.replace(pos, 1, " | ");
        pos += 3;
    }

    std::stringstream ss(processed_line);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool Grammar::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    int prod_id_counter = 0;
    bool is_first = true;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) continue;
        
        // Find '->'
        auto arrow_iter = std::find(tokens.begin(), tokens.end(), "->");
        if (arrow_iter == tokens.end()) continue; // Not a valid production line
        
        std::string lhs_str = tokens[0];
        int lhs_id = get_symbol_id(lhs_str);
        non_terminals.insert(lhs_id);

        if (is_first) {
            // Augmented grammar start
            start_symbol_id = get_symbol_id(lhs_str + "'");
            non_terminals.insert(start_symbol_id);
            Production aug_prod;
            aug_prod.id = prod_id_counter++;
            aug_prod.lhs = start_symbol_id;
            aug_prod.rhs.push_back(lhs_id);
            productions.push_back(aug_prod);
            is_first = false;
        }

        std::vector<int> current_rhs;
        for (auto it = arrow_iter + 1; it != tokens.end(); ++it) {
            if (*it == "|") {
                Production p;
                p.id = prod_id_counter++;
                p.lhs = lhs_id;
                p.rhs = current_rhs;
                productions.push_back(p);
                current_rhs.clear();
            } else {
                current_rhs.push_back(get_symbol_id(*it));
            }
        }
        // Push the last one
        Production p;
        p.id = prod_id_counter++;
        p.lhs = lhs_id;
        p.rhs = current_rhs;
        productions.push_back(p);
    }

    compute_first_sets();
    compute_follow_sets();
    return true;
}

const std::set<int>& Grammar::get_first_set(int symbol_id) const {
    auto it = first_sets.find(symbol_id);
    if (it != first_sets.end()) return it->second;
    static std::set<int> empty_set;
    return empty_set;
}

const std::set<int>& Grammar::get_follow_set(int symbol_id) const {
    auto it = follow_sets.find(symbol_id);
    if (it != follow_sets.end()) return it->second;
    static std::set<int> empty_set;
    return empty_set;
}

std::set<int> Grammar::compute_first_of_sequence(const std::vector<int>& seq) const {
    std::set<int> result;
    bool all_derive_epsilon = true;
    for (int sym : seq) {
        const auto& f_set = get_first_set(sym);
        for (int s : f_set) {
            if (s != EPSILON_ID) result.insert(s);
        }
        if (f_set.count(EPSILON_ID) == 0) {
            all_derive_epsilon = false;
            break;
        }
    }
    if (all_derive_epsilon) {
        result.insert(EPSILON_ID);
    }
    return result;
}

void Grammar::compute_first_sets() {
    // Initialize first sets
    for (int i = 0; i < (int)id_to_symbol.size(); ++i) {
        if (!is_non_terminal(i)) {
            first_sets[i].insert(i);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& prod : productions) {
            int lhs = prod.lhs;
            size_t old_size = first_sets[lhs].size();

            if (prod.rhs.empty() || (prod.rhs.size() == 1 && prod.rhs[0] == EPSILON_ID)) {
                first_sets[lhs].insert(EPSILON_ID);
            } else {
                bool all_derive_epsilon = true;
                for (int sym : prod.rhs) {
                    const auto& rhs_first = first_sets[sym];
                    for (int s : rhs_first) {
                        if (s != EPSILON_ID) first_sets[lhs].insert(s);
                    }
                    if (rhs_first.count(EPSILON_ID) == 0) {
                        all_derive_epsilon = false;
                        break;
                    }
                }
                if (all_derive_epsilon) {
                    first_sets[lhs].insert(EPSILON_ID);
                }
            }

            if (first_sets[lhs].size() > old_size) {
                changed = true;
            }
        }
    }
}

void Grammar::compute_follow_sets() {
    follow_sets[start_symbol_id].insert(END_MARKER_ID);

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& prod : productions) {
            int lhs = prod.lhs;
            for (size_t i = 0; i < prod.rhs.size(); ++i) {
                int sym = prod.rhs[i];
                if (!is_non_terminal(sym)) continue;

                size_t old_size = follow_sets[sym].size();
                
                if (sym == EPSILON_ID) continue; // @ doesn't have follow sets

                std::vector<int> beta(prod.rhs.begin() + i + 1, prod.rhs.end());
                std::set<int> first_beta = compute_first_of_sequence(beta);

                for (int f : first_beta) {
                    if (f != EPSILON_ID) {
                        follow_sets[sym].insert(f);
                    }
                }

                if (first_beta.count(EPSILON_ID) > 0 || beta.empty()) {
                    const auto& follow_lhs = follow_sets[lhs];
                    for (int f : follow_lhs) {
                        follow_sets[sym].insert(f);
                    }
                }

                if (follow_sets[sym].size() > old_size) {
                    changed = true;
                }
            }
        }
    }
}

void Grammar::print_grammar() const {
    for (const auto& prod : productions) {
        std::cout << get_symbol_str(prod.lhs) << " -> ";
        if (prod.rhs.empty() || (prod.rhs.size() == 1 && prod.rhs[0] == EPSILON_ID)) {
            std::cout << "@";
        } else {
            for (int sym : prod.rhs) {
                std::cout << get_symbol_str(sym) << " ";
            }
        }
        std::cout << std::endl;
    }
}

void Grammar::print_first_sets() const {
    for (int i = 0; i < (int)id_to_symbol.size(); ++i) {
        if (!is_non_terminal(i)) continue;
        if (i == start_symbol_id) continue; // usually don't print augmented start
        std::cout << "First(" << get_symbol_str(i) << ") = { ";
        for (int sym : first_sets.at(i)) {
            std::cout << get_symbol_str(sym) << " ";
        }
        std::cout << "}\n";
    }
}

void Grammar::print_follow_sets() const {
    for (int i = 0; i < (int)id_to_symbol.size(); ++i) {
        if (!is_non_terminal(i)) continue;
        if (i == start_symbol_id) continue;
        std::cout << "Follow(" << get_symbol_str(i) << ") = { ";
        for (int sym : follow_sets.at(i)) {
            std::cout << get_symbol_str(sym) << " ";
        }
        std::cout << "}\n";
    }
}
