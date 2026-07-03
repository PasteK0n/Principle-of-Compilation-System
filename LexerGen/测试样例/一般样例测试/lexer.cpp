#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>

using namespace std;

const int STATE_COUNT = 16;
bool is_accept[STATE_COUNT] = {false,false,false,true,true,true,true,true,true,true,true,true,true,true,true,true};

int token_id[STATE_COUNT] = {-1,-1,-1,100,101,102,200,201,202,203,204,205,206,207,208,209};

string token_name[STATE_COUNT] = {"","","","num100","ID101","comment102","assign200","plus201","minus202","times203","over204","eq205","lt206","lparen207","rparen208","semi209"};

int get_keyword_id(const string& lexeme) {
    if(lexeme == "else") return 105;
    if(lexeme == "end") return 106;
    if(lexeme == "if") return 103;
    if(lexeme == "read") return 109;
    if(lexeme == "repeat") return 107;
    if(lexeme == "then") return 104;
    if(lexeme == "until") return 108;
    if(lexeme == "write") return 110;
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
            if ((ch == ':')) {
                pos++; // 隐式转移到状态 0
                if (pos < code.length()) {
                    char ch = code[pos];
                    if ((ch == '=')) {
                        pos++; // 隐式转移到状态 6
                        last_accept_state = 6;
                        last_accept_pos = pos;
                        // 状态 6 是终态，更新最近接受的位置和状态
                    }
                }
            } else if ((ch == '{')) {
                pos++; // 隐式转移到状态 2
                while (pos < code.length()) {
                    char ch = code[pos];
                    if ((ch == '}')) {
                        pos++; // 隐式转移到状态 5
                        last_accept_state = 5;
                        last_accept_pos = pos;
                        // 状态 5 是终态，更新最近接受的位置和状态
                        break;
                    } else if (((ch >= '\x01' && ch <= '|') || (ch >= '~' && ch <= '\xff'))) {
                        pos++;
                    } else {
                        break;
                    }
                }
            } else if (((ch >= '0' && ch <= '9'))) {
                pos++; // 隐式转移到状态 3
                last_accept_state = 3;
                last_accept_pos = pos;
                // 状态 3 是终态，更新最近接受的位置和状态
                while (pos < code.length()) {
                    char ch = code[pos];
                    if (((ch >= '0' && ch <= '9'))) {
                        pos++;
                        last_accept_pos = pos; // 保持在状态 3 且更新终态位置
                    } else {
                        break;
                    }
                }
            } else if (((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))) {
                pos++; // 隐式转移到状态 4
                last_accept_state = 4;
                last_accept_pos = pos;
                // 状态 4 是终态，更新最近接受的位置和状态
                while (pos < code.length()) {
                    char ch = code[pos];
                    if (((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))) {
                        pos++;
                        last_accept_pos = pos; // 保持在状态 4 且更新终态位置
                    } else {
                        break;
                    }
                }
            } else if ((ch == '+')) {
                pos++; // 隐式转移到状态 7
                last_accept_state = 7;
                last_accept_pos = pos;
                // 状态 7 是终态，更新最近接受的位置和状态
            } else if ((ch == '-')) {
                pos++; // 隐式转移到状态 8
                last_accept_state = 8;
                last_accept_pos = pos;
                // 状态 8 是终态，更新最近接受的位置和状态
            } else if ((ch == '*')) {
                pos++; // 隐式转移到状态 9
                last_accept_state = 9;
                last_accept_pos = pos;
                // 状态 9 是终态，更新最近接受的位置和状态
            } else if ((ch == '/')) {
                pos++; // 隐式转移到状态 10
                last_accept_state = 10;
                last_accept_pos = pos;
                // 状态 10 是终态，更新最近接受的位置和状态
            } else if ((ch == '=')) {
                pos++; // 隐式转移到状态 11
                last_accept_state = 11;
                last_accept_pos = pos;
                // 状态 11 是终态，更新最近接受的位置和状态
            } else if ((ch == '<')) {
                pos++; // 隐式转移到状态 12
                last_accept_state = 12;
                last_accept_pos = pos;
                // 状态 12 是终态，更新最近接受的位置和状态
            } else if ((ch == '(')) {
                pos++; // 隐式转移到状态 13
                last_accept_state = 13;
                last_accept_pos = pos;
                // 状态 13 是终态，更新最近接受的位置和状态
            } else if ((ch == ')')) {
                pos++; // 隐式转移到状态 14
                last_accept_state = 14;
                last_accept_pos = pos;
                // 状态 14 是终态，更新最近接受的位置和状态
            } else if ((ch == ';')) {
                pos++; // 隐式转移到状态 15
                last_accept_state = 15;
                last_accept_pos = pos;
                // 状态 15 是终态，更新最近接受的位置和状态
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
