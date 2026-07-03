/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        lexer_gen.cpp
 * @brief       用于实现从最小化DFA生成C++词法分析器代码的模块，核心功能是将DFA状态转移表和接受状态信息转换为可编译的C++源代码
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
 * V1.1.0      2026-05-15    陈乐豪      2. 实现minic语言分析功能，并且修改生成的分析器运行期逻辑，提高容错和健壮性
 ******************************************************************************/
#include "lexer_gen.h"
#include <sstream>
#include <set>
#include <iomanip>
#include <vector>
#include <map>

using namespace std;

struct CharRange {
    int start;
    int end;
};

vector<CharRange> find_ranges(const set<int>& chars) {
    vector<CharRange> ranges;
    if (chars.empty()) return ranges;
    
    auto it = chars.begin();
    int start = *it;
    int prev = *it;
    it++;
    for (; it != chars.end(); it++) {
        if (*it == prev + 1) {
            prev = *it;
        } else {
            ranges.push_back({start, prev});
            start = *it;
            prev = *it;
        }
    }
    ranges.push_back({start, prev});
    return ranges;
}

string format_char(unsigned char c) {
    if (c == '\n') return "'\\n'";
    if (c == '\t') return "'\\t'";
    if (c == '\r') return "'\\r'";
    if (c == '\\') return "'\\\\'";
    if (c == '\'') return "'\\''";
    if (c >= 32 && c <= 126) {
        return string("'") + (char)c + "'";
    }
    stringstream ss;
    ss << "'\\x" << setfill('0') << setw(2) << hex << (int)c << "'";
    return ss.str();
}

string build_condition(const set<int>& chars, const string& var_name = "ch") {
    vector<CharRange> ranges = find_ranges(chars);
    if (ranges.empty()) return "false";
    
    stringstream ss;
    ss << "(";
    for (size_t i = 0; i < ranges.size(); i++) {
        if (i > 0) ss << " || ";
        if (ranges[i].start == ranges[i].end) {
            ss << var_name << " == " << format_char(ranges[i].start);
        } else if (ranges[i].end - ranges[i].start == 1) {
            ss << var_name << " == " << format_char(ranges[i].start) << " || " 
               << var_name << " == " << format_char(ranges[i].end);
        } else {
            ss << "(" << var_name << " >= " << format_char(ranges[i].start) 
               << " && " << var_name << " <= " << format_char(ranges[i].end) << ")";
        }
    }
    ss << ")";
    return ss.str();
}

string indent(int n) {
    return string(n * 4, ' ');
}

string generate_state_code(DFA* dfa, int u, int level, set<int>& path) {
    if (path.count(u)) {
        stringstream err;
        err << indent(level) << "// 警告：检测到状态 " << u << " 存在环路\n";
        return err.str();
    }
    path.insert(u);
    
    stringstream cpp;
    
    // 1. 如果 u 是终态，记录最近接受的终态和对应的 Token 位置
    if (dfa->states[u]->is_accept) {
        cpp << indent(level) << "last_accept_state = " << u << ";\n";
        cpp << indent(level) << "last_accept_pos = pos;\n";
        cpp << indent(level) << "// 状态 " << u << " 是终态，更新最近接受的位置和状态\n";
    }
    
    // 2. 收集 u 的转移
    set<int> self_chars;
    vector<int> other_targets;
    map<int, set<int>> target_to_chars;
    
    for (auto& transition : dfa->states[u]->transitions) {
        int c = transition.first;
        int v = transition.second;
        if (v != -1) {
            target_to_chars[v].insert(c);
        }
    }
    
    if (target_to_chars.count(u)) {
        self_chars = target_to_chars[u];
    }
    for (auto& pair : target_to_chars) {
        if (pair.first != u) {
            other_targets.push_back(pair.first);
        }
    }
    
    // 3. 根据转移类型生成 nested control flow
    if (!self_chars.empty()) {
        // 存在自环
        if (other_targets.empty()) {
            cpp << indent(level) << "while (pos < code.length()) {\n";
            cpp << indent(level + 1) << "char ch = code[pos];\n";
            cpp << indent(level + 1) << "if (" << build_condition(self_chars, "ch") << ") {\n";
            cpp << indent(level + 2) << "pos++;\n";
            if (dfa->states[u]->is_accept) {
                cpp << indent(level + 2) << "last_accept_pos = pos; // 保持在状态 " << u << " 且更新终态位置\n";
            }
            cpp << indent(level + 1) << "} else {\n";
            cpp << indent(level + 2) << "break;\n";
            cpp << indent(level + 1) << "}\n";
            cpp << indent(level) << "}\n";
        } else {
            cpp << indent(level) << "while (pos < code.length()) {\n";
            cpp << indent(level + 1) << "char ch = code[pos];\n";
            bool first = true;
            for (int v : other_targets) {
                string cond = build_condition(target_to_chars[v], "ch");
                if (first) {
                    cpp << indent(level + 1) << "if (" << cond << ") {\n";
                    first = false;
                } else {
                    cpp << indent(level + 1) << "} else if (" << cond << ") {\n";
                }
                cpp << indent(level + 2) << "pos++; // 隐式转移到状态 " << v << "\n";
                cpp << generate_state_code(dfa, v, level + 2, path);
                cpp << indent(level + 2) << "break;\n";
            }
            string self_cond = build_condition(self_chars, "ch");
            cpp << indent(level + 1) << "} else if (" << self_cond << ") {\n";
            cpp << indent(level + 2) << "pos++;\n";
            if (dfa->states[u]->is_accept) {
                cpp << indent(level + 2) << "last_accept_pos = pos; // 保持在状态 " << u << " 且更新终态位置\n";
            }
            cpp << indent(level + 1) << "} else {\n";
            cpp << indent(level + 2) << "break;\n";
            cpp << indent(level + 1) << "}\n";
            cpp << indent(level) << "}\n";
        }
    } else {
        // 无自环，仅有前向转移
        if (!other_targets.empty()) {
            cpp << indent(level) << "if (pos < code.length()) {\n";
            cpp << indent(level + 1) << "char ch = code[pos];\n";
            bool first = true;
            for (int v : other_targets) {
                string cond = build_condition(target_to_chars[v], "ch");
                if (first) {
                    cpp << indent(level + 1) << "if (" << cond << ") {\n";
                    first = false;
                } else {
                    cpp << indent(level + 1) << "} else if (" << cond << ") {\n";
                }
                cpp << indent(level + 2) << "pos++; // 隐式转移到状态 " << v << "\n";
                cpp << generate_state_code(dfa, v, level + 2, path);
            }
            cpp << indent(level + 1) << "}\n";
            cpp << indent(level) << "}\n";
        }
    }
    
    path.erase(u);
    return cpp.str();
}

string generate_lexer_cpp(DFA* min_dfa, const map<string, int>& keywords) {
    stringstream cpp;
    
    int n = min_dfa->states.size();
    
    cpp << "#include <iostream>\n";
    cpp << "#include <fstream>\n";
    cpp << "#include <string>\n";
    cpp << "#include <vector>\n";
    cpp << "#include <cctype>\n\n";
    cpp << "using namespace std;\n\n";
    
    cpp << "const int STATE_COUNT = " << n << ";\n";
    
    cpp << "bool is_accept[STATE_COUNT] = {";
    for(int i=0; i<n; i++) cpp << (min_dfa->states[i]->is_accept ? "true" : "false") << (i==n-1 ? "" : ",");
    cpp << "};\n\n";
    
    cpp << "int token_id[STATE_COUNT] = {";
    for(int i=0; i<n; i++) cpp << min_dfa->states[i]->token_id << (i==n-1 ? "" : ",");
    cpp << "};\n\n";
    
    cpp << "string token_name[STATE_COUNT] = {";
    for(int i=0; i<n; i++) cpp << "\"" << min_dfa->states[i]->token_name << "\"" << (i==n-1 ? "" : ",");
    cpp << "};\n\n";
    
    cpp << "int get_keyword_id(const string& lexeme) {\n";
    for(auto& kv : keywords) {
        cpp << "    if(lexeme == \"" << kv.first << "\") return " << kv.second << ";\n";
    }
    cpp << "    return -1;\n";
    cpp << "}\n\n";
    
    set<int> path;
    string match_code = generate_state_code(min_dfa, min_dfa->start_state, 2, path);
    
    cpp << R"(int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <source_file>" << endl;
        return 1;
    }
    
    ifstream in(argv[1]);
    if (!in) {
        cerr << "Error opening file " << argv[1] << endl;
        return 1;
    }
    
    string code((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    size_t pos = 0;
    
    string out_name = string(argv[1]);
    size_t dot_pos = out_name.find_last_of('.');
    if(dot_pos != string::npos) out_name = out_name.substr(0, dot_pos);
    out_name += ".lex";
    
    ofstream out(out_name);
    
    while(pos < code.length()) {
        // ================= 【核心修复：顶层双重预处理】 =================
        bool pre_processed = true;
        while (pre_processed && pos < code.length()) {
            pre_processed = false;

            // 1. 无条件跳过空白符
            while (pos < code.length() && isspace(static_cast<unsigned char>(code[pos]))) {
                pos++;
                pre_processed = true;
            }

            // 2. 识别并静默跳过 MiniC 语言的行注释 (以 // 开头)
            if (pos + 1 < code.length() && code[pos] == '/' && code[pos + 1] == '/') {
                pos += 2; // 略过 "//"
                while (pos < code.length() && code[pos] != '\n' && code[pos] != '\r') {
                    pos++; // 吞掉整行注释内容
                }
                pre_processed = true;
                continue;
            }

            // 3. 识别并静默跳过 TINY 语言的花括号块注释 (以 { 开头)
            if (pos < code.length() && code[pos] == '{') {
                // 如果需要支持 TINY 块注释，可以取消下面代码的注释
                /*
                size_t j = pos + 1;
                while (j < code.length() && code[j] != '}') {
                    j++;
                }
                if (j < code.length() && code[j] == '}') {
                    pos = j + 1; // 成功吞掉整段 TINY 块注释
                    pre_processed = true;
                    continue;
                }
                */
            }
        }
        
        if (pos >= code.length()) break;
        // =============================================================
        
        size_t start_pos = pos;
        int last_accept_state = -1;
        size_t last_accept_pos = pos;
        
        // ================= 【直接逻辑控制匹配开始】 =================
)";

    cpp << match_code;

    cpp << R"(        // ================= 【直接逻辑控制匹配结束】 =================
        
        // 3. 匹配结果分流处理
        if (last_accept_state != -1) {
            string lexeme = code.substr(start_pos, last_accept_pos - start_pos);
            pos = last_accept_pos; // 推进全局主指针
            
            // 过滤注释
            if (token_name[last_accept_state].find("comment") != string::npos) {
                continue;
            }
            
            int final_token_id = token_id[last_accept_state];
            int kw_id = get_keyword_id(lexeme);
            
            if(kw_id != -1) {
                final_token_id = kw_id;
            }
            
            // 4. 标准输出规范
            if (kw_id != -1) {
                out << final_token_id << " ";
                cout << final_token_id << " ";
            } else if (token_name[last_accept_state].find("ID") != string::npos || 
                       token_name[last_accept_state].find("id") != string::npos) {
                out << final_token_id << " \"" << lexeme << "\" ";
                cout << final_token_id << " \"" << lexeme << "\" ";
            } else if (token_name[last_accept_state].find("num") != string::npos || 
                       token_name[last_accept_state].find("NUM") != string::npos || 
                       token_name[last_accept_state].find("digit") != string::npos) {
                out << final_token_id << " " << lexeme << " ";
                cout << final_token_id << " " << lexeme << " ";
            } else {
                out << final_token_id << " ";
                cout << final_token_id << " ";
            }
        } else {
            // 5. 健壮的错误恢复
            cerr << "Lexical error at character: " << code[start_pos] << endl;
            pos = start_pos + 1;
        }
    }
    cout << endl;
    return 0;
}
)";

    return cpp.str();
}