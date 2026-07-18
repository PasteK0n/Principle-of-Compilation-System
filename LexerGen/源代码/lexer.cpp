#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>

using namespace std;

const int STATE_COUNT = 11;
bool is_accept[STATE_COUNT] = {false,false,false,false,false,false,false,false,false,false,true};

int token_id[STATE_COUNT] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,101};

string token_name[STATE_COUNT] = {"","","","","","","","","","","ID101"};

int get_keyword_id(const string& lexeme) {
    return -1;
}

int main(int argc, char** argv) {
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
        if (pos < code.length()) {
            char ch = code[pos];
            if ((ch == 'l')) {
                pos++; // 隐式转移到状态 8
                if (pos < code.length()) {
                    char ch = code[pos];
                    if ((ch == 'e')) {
                        pos++; // 隐式转移到状态 0
                        if (pos < code.length()) {
                            char ch = code[pos];
                            if ((ch == 't')) {
                                pos++; // 隐式转移到状态 1
                                if (pos < code.length()) {
                                    char ch = code[pos];
                                    if ((ch == 't')) {
                                        pos++; // 隐式转移到状态 9
                                        if (pos < code.length()) {
                                            char ch = code[pos];
                                            if ((ch == 'e')) {
                                                pos++; // 隐式转移到状态 3
                                                if (pos < code.length()) {
                                                    char ch = code[pos];
                                                    if ((ch == 'r')) {
                                                        pos++; // 隐式转移到状态 10
                                                        last_accept_state = 10;
                                                        last_accept_pos = pos;
                                                        // 状态 10 是终态，更新最近接受的位置和状态
                                                        if (pos < code.length()) {
                                                            char ch = code[pos];
                                                            if ((ch == 'd')) {
                                                                pos++; // 隐式转移到状态 6
                                                                if (pos < code.length()) {
                                                                    char ch = code[pos];
                                                                    if ((ch == 'i')) {
                                                                        pos++; // 隐式转移到状态 7
                                                                        if (pos < code.length()) {
                                                                            char ch = code[pos];
                                                                            if ((ch == 'g')) {
                                                                                pos++; // 隐式转移到状态 5
                                                                                if (pos < code.length()) {
                                                                                    char ch = code[pos];
                                                                                    if ((ch == 'i')) {
                                                                                        pos++; // 隐式转移到状态 2
                                                                                        if (pos < code.length()) {
                                                                                            char ch = code[pos];
                                                                                            if ((ch == 't')) {
                                                                                                pos++; // 隐式转移到状态 10
                                                                                                // 警告：检测到状态 10 存在环路
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            } else if ((ch == 'l')) {
                                                                pos++; // 隐式转移到状态 8
                                                                // 警告：检测到状态 8 存在环路
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        // ================= 【直接逻辑控制匹配结束】 =================
        
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
