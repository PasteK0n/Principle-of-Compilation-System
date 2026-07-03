/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        automata.cpp
 * @brief       用于实现regex到NFA、NFA到DFA、DFA最小化的核心算法模块
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

#include "automata.h"
#include <queue>
#include <algorithm>
#include <sstream>

using namespace std;
/* 
string NFA::to_string() const {
    stringstream ss;
    ss << "NFA States: " << states.size() << "\n";
    ss << "Start State: " << start_state << "\n";
    for(auto s : states) {
        ss << "State " << s->id;
        if(s->is_accept) ss << " [Accept: " << s->token_name << "(" << s->token_id << ")]";
        ss << "\n    Transitions: ";
        map<int, vector<int>> state_to_chars;
        for(auto& t : s->transitions) {
            state_to_chars[t.second].push_back(t.first);
        }
        for(auto& pair : state_to_chars) {
            ss << "{";
            bool first = true;
            for(int c : pair.second) {
                if(!first) ss << ",";
                first = false;
                if(c == EPSILON) ss << "EPSILON";
                else if(c == '\n') ss << "\\n";
                else if(c == '\r') ss << "\\r";
                else if(c == '\t') ss << "\\t";
                else ss << "'" << (char)c << "'";
            }
            ss << "}->" << pair.first << "  ";
        }
        ss << "\n";
    }
    return ss.str();
}
*/
string NFA::to_string() const {
    stringstream ss;
    ss << "=================================== NFA CONFIGURATION ===================================\n";
    ss << " Total States: " << states.size() << " | Start State: " << start_state << "\n\n";

    // 1. 收集所有出现过的转移字符（去重排序），作为表格的表头
    set<int> active_chars;
    for (auto s : states) {
        for (auto& t : s->transitions) {
            active_chars.insert(t.first);
        }
    }

    // 2. 打印表头边界
    ss << "+-----------+-------------------------+";
    for (size_t i = 0; i < active_chars.size(); ++i) ss << "-------+";
    ss << "\n";

    // 3. 打印表头文字
    // 格式：| State     | Type/Token              |  ch1  |  ch2  | ...
    char buf[128];
    sprintf(buf, "| %-9s | %-23s |", "State", "Type/Accept Token");
    ss << buf;
    for (int c : active_chars) {
        string ch_str;
        if (c == EPSILON) ch_str = "EPS";
        else if (c == '\n') ch_str = "\\n";
        else if (c == '\r') ch_str = "\\r";
        else if (c == '\t') ch_str = "\\t";
        else if (c == ' ')  ch_str = "SPC";
        else ch_str = string(1, (char)c);
        
        sprintf(buf, " %-5s |", ch_str.substr(0, 5).c_str());
        ss << buf;
    }
    ss << "\n";

    // 4. 打印表头下方的分割线
    ss << "+-----------+-------------------------+";
    for (size_t i = 0; i < active_chars.size(); ++i) ss << "-------+";
    ss << "\n";

    // 5. 逐行打印每个状态的转移
    for (auto s : states) {
        // 状态属性列
        string token_info = "";
        if (s->is_accept) {
            token_info = s->token_name + "(" + std::to_string(s->token_id) + ")";
        }
        sprintf(buf, "| %-9d | %-23s |", s->id, token_info.substr(0, 23).c_str());
        ss << buf;

        // 整理当前状态对各字符的转移目标集
        map<int, vector<int>> char_to_targets;
        for (auto& t : s->transitions) {
            char_to_targets[t.first].push_back(t.second);
        }

        // 填充每个字符列的目标状态
        for (int c : active_chars) {
            if (char_to_targets.count(c)) {
                string targets_str = "";
                for (size_t idx = 0; idx < char_to_targets[c].size(); ++idx) {
                    if (idx > 0) targets_str += ",";
                    targets_str += std::to_string(char_to_targets[c][idx]);
                }
                // 如果目标过多，截断显示
                if (targets_str.length() > 5) targets_str = targets_str.substr(0, 4) + "..";
                sprintf(buf, " %-5s |", targets_str.c_str());
            } else {
                sprintf(buf, " %-5s |", "-");
            }
            ss << buf;
        }
        ss << "\n";
    }

    // 6. 打印表底边界
    ss << "+-----------+-------------------------+";
    for (size_t i = 0; i < active_chars.size(); ++i) ss << "-------+";
    ss << "\n";

    return ss.str();
}

/*
string DFA::to_string() const {
    stringstream ss;
    ss << "DFA States: " << states.size() << "\n";
    ss << "Start State: " << start_state << "\n";
    
    for(auto s : states) {
        ss << "State " << s->id;
        if(s->is_accept) ss << " [Accept: " << s->token_name << "(" << s->token_id << ")]";
        ss << " (from NFA {";
        bool first_nfa = true;
        for(int u : s->nfa_states) {
            if(!first_nfa) ss << ",";
            first_nfa = false;
            ss << u;
        }
        ss << "})\n    Transitions: ";
        map<int, vector<int>> state_to_chars;
        for(auto& t : s->transitions) {
            state_to_chars[t.second].push_back(t.first);
        }
        for(auto& pair : state_to_chars) {
            ss << "{";
            bool first_c = true;
            for(int c : pair.second) {
                if(!first_c) ss << ",";
                first_c = false;
                if(c == '\n') ss << "\\n";
                else if(c == '\r') ss << "\\r";
                else if(c == '\t') ss << "\\t";
                else ss << "'" << (char)c << "'";
            }
            ss << "}->" << pair.first << "  ";
        }
        ss << "\n";
    }
    return ss.str();
}
*/
string DFA::to_string() const {
    stringstream ss;
    ss << "=================================== DFA CONFIGURATION ===================================\n";
    ss << " Total States: " << states.size() << " | Start State: " << start_state << "\n\n";

    // 1. 收集所有出现过的转移字符（去重排序），作为表格的表头
    set<int> active_chars;
    for (auto s : states) {
        for (auto& t : s->transitions) {
            active_chars.insert(t.first);
        }
    }

    // 2. 打印表头边界
    ss << "+-----------+-------------------------+----------------------+";
    for (size_t i = 0; i < active_chars.size(); ++i) ss << "-------+";
    ss << "\n";

    // 3. 打印表头文字
    char buf[128];
    sprintf(buf, "| %-9s | %-23s | %-20s |", "DFA State", "Type/Accept Token", "From NFA Set");
    ss << buf;
    for (int c : active_chars) {
        string ch_str;
        if (c == '\n') ch_str = "\\n";
        else if (c == '\r') ch_str = "\\r";
        else if (c == '\t') ch_str = "\\t";
        else if (c == ' ')  ch_str = "SPC";
        else ch_str = string(1, (char)c);
        
        sprintf(buf, " %-5s |", ch_str.substr(0, 5).c_str());
        ss << buf;
    }
    ss << "\n";

    // 4. 打印表头下方的分割线
    ss << "+-----------+-------------------------+----------------------+";
    for (size_t i = 0; i < active_chars.size(); ++i) ss << "-------+";
    ss << "\n";

    // 5. 逐行打印每个 DFA 状态的转移
    for (auto s : states) {
        // 接受状态 Token 信息
        string token_info = "";
        if (s->is_accept) {
            token_info = s->token_name + "(" + std::to_string(s->token_id) + ")";
        }

        // 来源的 NFA 状态映射集集合化输出
        string nfa_set_str = "{";
        bool first_nfa = true;
        for (int u : s->nfa_states) {
            if (!first_nfa) nfa_set_str += ",";
            first_nfa = false;
            nfa_set_str += std::to_string(u);
        }
        nfa_set_str += "}";
        if (nfa_set_str.length() > 20) nfa_set_str = nfa_set_str.substr(0, 17) + "...";

        sprintf(buf, "| %-9d | %-23s | %-20s |", s->id, token_info.substr(0, 23).c_str(), nfa_set_str.c_str());
        ss << buf;

        // 填充每个字符列的目标状态 (DFA 是单目标转移，直接取出即可)
        for (int c : active_chars) {
            if (s->transitions.count(c)) {
                sprintf(buf, " %-5d |", s->transitions[c]);
            } else {
                sprintf(buf, " %-5s |", "-");
            }
            ss << buf;
        }
        ss << "\n";
    }

    // 6. 打印表底边界
    ss << "+-----------+-------------------------+----------------------+";
    for (size_t i = 0; i < active_chars.size(); ++i) ss << "-------+";
    ss << "\n";

    return ss.str();
}


// 计算 epsilon 闭包，给定一个 NFA 和一个状态集合，返回从这些状态出发通过 epsilon 转移能够到达的所有状态的集合
set<int> epsilon_closure(NFA* nfa, const set<int>& states) {
    set<int> closure = states;
    vector<int> stack(states.begin(), states.end());
    while(!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        for(auto& edge : nfa->states[u]->transitions) {
            if(edge.first == EPSILON) {
                if(closure.find(edge.second) == closure.end()) {
                    closure.insert(edge.second);
                    stack.push_back(edge.second);
                }
            }
        }
    }
    return closure;
}


// 计算 move 集，给定一个 NFA、一个状态集合和一个输入字符，返回从这些状态出发通过该输入字符能够到达的所有状态的集合
set<int> move_set(NFA* nfa, const set<int>& states, int c) {
    set<int> res;
    for(int u : states) {
        for(auto& edge : nfa->states[u]->transitions) {
            if(edge.first == c) {
                res.insert(edge.second);
            }
        }
    }
    return res;
}


// 从 NFA 转换到 DFA，使用子集构造算法，维护一个从状态集合到 DFA 状态的映射，同时使用一个队列来处理未标记的状态集合，直到所有状态集合都被处理完毕
DFA* nfa_to_dfa(NFA* nfa) {
    DFA* dfa = new DFA();
    map<set<int>, DFAState*> dfa_states;
    vector<set<int>> unmark;
    
    set<int> start_set = epsilon_closure(nfa, {nfa->start_state});
    DFAState* d0 = dfa->new_state();
    d0->nfa_states = start_set;
    dfa->start_state = d0->id;
    dfa_states[start_set] = d0;
    unmark.push_back(start_set);
    
    while(!unmark.empty()) {
        auto cur_set = unmark.back();
        unmark.pop_back();
        DFAState* d_cur = dfa_states[cur_set];
        
        int min_token_id = 1e9;
        for(int u : cur_set) {
            if(nfa->states[u]->is_accept) {
                d_cur->is_accept = true;
                if(nfa->states[u]->token_id < min_token_id) {
                    min_token_id = nfa->states[u]->token_id;
                    d_cur->token_id = min_token_id;
                    d_cur->token_name = nfa->states[u]->token_name;
                }
            }
        }
        
        for(int c=1; c<256; c++) {
            set<int> t = move_set(nfa, cur_set, c);
            if(t.empty()) continue;
            set<int> U = epsilon_closure(nfa, t);
            
            if(dfa_states.find(U) == dfa_states.end()) {
                DFAState* d_new = dfa->new_state();
                d_new->nfa_states = U;
                dfa_states[U] = d_new;
                unmark.push_back(U);
            }
            d_cur->add_transition(c, dfa_states[U]->id);
        }
    }
    return dfa;
}


// DFA 最小化，使用 Hopcroft 算法，首先根据接受状态和非接受状态将状态分成两个初始集合，然后迭代地细分这些集合，直到无法再细分为止，最后构建一个新的 DFA，其中每个集合对应一个新的状态
DFA* minimize_dfa(DFA* dfa) {
    int n = dfa->states.size();
    if(n == 0) return new DFA();
    
    vector<int> group(n);
    vector<vector<int>> partitions;
    
    // Initialize partitions based on acceptance and token ID
    map<pair<bool, int>, vector<int>> init_groups;
    for(int i=0; i<n; i++) {
        init_groups[{dfa->states[i]->is_accept, dfa->states[i]->token_id}].push_back(i);
    }
    int g_idx = 0;
    for(auto& kv : init_groups) {
        for(int u : kv.second) group[u] = g_idx;
        partitions.push_back(kv.second);
        g_idx++;
    }
    
    bool changed = true;
    while(changed) {
        changed = false;
        vector<vector<int>> new_partitions;
        int new_g_idx = 0;
        vector<int> new_group(n);
        
        for(auto& p : partitions) {
            map<vector<int>, vector<int>> sig_map;
            for(int u : p) {
                vector<int> sig;
                for(int c=1; c<256; c++) {
                    if(dfa->states[u]->transitions.count(c)) {
                        sig.push_back(group[dfa->states[u]->transitions[c]]);
                    } else {
                        sig.push_back(-1);
                    }
                }
                sig_map[sig].push_back(u);
            }
            if(sig_map.size() > 1) changed = true;
            for(auto& kv : sig_map) {
                for(int u : kv.second) new_group[u] = new_g_idx;
                new_partitions.push_back(kv.second);
                new_g_idx++;
            }
        }
        partitions = new_partitions;
        group = new_group;
    }
    
    DFA* min_dfa = new DFA();
    for(int i=0; i<partitions.size(); i++) {
        min_dfa->new_state();
    }
    min_dfa->start_state = group[dfa->start_state];
    
    for(int i=0; i<partitions.size(); i++) {
        int rep = partitions[i][0];
        DFAState* min_s = min_dfa->states[i];
        DFAState* orig_s = dfa->states[rep];
        
        min_s->is_accept = orig_s->is_accept;
        min_s->token_id = orig_s->token_id;
        min_s->token_name = orig_s->token_name;
        
        for(int u : partitions[i]) {
            min_s->nfa_states.insert(dfa->states[u]->nfa_states.begin(), dfa->states[u]->nfa_states.end());
        }
        
        for(auto& edge : orig_s->transitions) {
            int to_group = group[edge.second];
            min_s->add_transition(edge.first, to_group);
        }
    }
    
    for(int i=0; i<min_dfa->states.size(); i++) {
        if(min_dfa->states[i]->is_accept) {
            min_dfa->accept_states.push_back(i);
        }
    }
    
    return min_dfa;
}
