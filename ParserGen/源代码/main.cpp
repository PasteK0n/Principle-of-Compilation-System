/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        main.cpp
 * @brief       主程序入口，包含Win32 GUI实现和事件处理逻辑，负责用户交互和调用语法分析器核心功能
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
#include <windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <iostream>
#include "grammar.h"
#include "lalr_automata.h"
#include "parser_engine.h"

using namespace std;

// UI Control IDs
#define ID_BTN_LOAD_BNF 101
#define ID_BTN_CALC_FF 102
#define ID_BTN_LR1_DFA 103
#define ID_BTN_LALR_DFA 104
#define ID_BTN_LALR_TABLE 105
#define ID_BTN_LOAD_LEX 106
#define ID_BTN_RUN_PARSER 107
#define ID_BTN_RESET_MAP 108
#define ID_BTN_IMPORT_REGEX 109

HWND hEditBNF, hEditTokenMap, hEditLex, hEditOut;

// Parse regular expression file and construct token ID map
map<int, string> import_token_map_from_regex(const string& regex_content) {
    map<int, string> token_map;
    stringstream ss(regex_content);
    string line;
    while (getline(ss, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == string::npos) continue;
        line = line.substr(start);
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != string::npos) line = line.substr(0, end + 1);
        
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == string::npos) continue;
        
        string name = line.substr(0, eq);
        string pattern = line.substr(eq + 1);
        
        size_t n_start = name.find_first_not_of(" \t\r\n");
        if (n_start != string::npos) {
            size_t n_end = name.find_last_not_of(" \t\r\n");
            name = name.substr(n_start, n_end - n_start + 1);
        }
        size_t p_start = pattern.find_first_not_of(" \t\r\n");
        if (p_start != string::npos) {
            size_t p_end = pattern.find_last_not_of(" \t\r\n");
            pattern = pattern.substr(p_start, p_end - p_start + 1);
        }
        
        string base;
        string num_str;
        string suffix;
        bool has_num = false;
        for (char c : name) {
            if (isdigit(c)) {
                num_str += c;
                has_num = true;
            } else if (has_num) {
                suffix += c;
            } else {
                base += c;
            }
        }
        
        if (!has_num) {
            continue;
        }
        
        int token_id = stoi(num_str);
        if (base.find("keyword") != string::npos || suffix == "B") {
            stringstream pat_ss(pattern);
            string part;
            while (getline(pat_ss, part, '|')) {
                size_t pt_start = part.find_first_not_of(" \t\r\n");
                if (pt_start == string::npos) continue;
                size_t pt_end = part.find_last_not_of(" \t\r\n");
                string kw = part.substr(pt_start, pt_end - pt_start + 1);
                token_map[token_id++] = kw;
            }
        } else {
            string base_lower = base;
            for (char& c : base_lower) c = tolower(c);
            
            if (base_lower == "id" || base_lower == "identifier") {
                token_map[token_id] = "ID";
            } else if (base_lower == "num" || base_lower == "number") {
                token_map[token_id] = "NUM";
            } else {
                string sym;
                for (size_t i = 0; i < pattern.length(); ++i) {
                    if (pattern[i] == '\\' && i + 1 < pattern.length()) {
                        sym += pattern[i+1];
                        i++;
                    } else {
                        sym += pattern[i];
                    }
                }
                token_map[token_id] = sym;
            }
        }
    }
    return token_map;
}

Grammar* current_grammar = nullptr;
LALRAutomata* current_automata = nullptr;
ParserEngine* current_engine = nullptr;

string current_bnf_path = "";
string current_lex_path = "";

// Helper: Get text from a Win32 edit control
string get_edit_text(HWND hwnd) {
    int len = GetWindowTextLengthA(hwnd);
    if (len == 0) return "";
    char* buf = new char[len + 1];
    GetWindowTextA(hwnd, buf, len + 1);
    string s = buf;
    delete[] buf;
    return s;
}

// Helper: Set text with standard Windows CRLF carriage returns
void set_edit_text(HWND hwnd, const string& text) {
    string win_text;
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
            win_text += "\r\n";
        } else {
            win_text += text[i];
        }
    }
    SetWindowTextA(hwnd, win_text.c_str());
}

// Helper: File dialog for loading files
string open_file_dialog(HWND owner, const char* filter) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return string(szFile);
    }
    return "";
}

// Helper: Read entire file content
string read_file(const string& path) {
    ifstream in(path);
    if (!in) return "";
    return string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
}

// Helper: Format First/Follow sets for window presentation
string format_first_follow(Grammar* g) {
    stringstream ss;
    ss << "=== FIRST SETS ===\n";
    for (int i = 0; i < 2000; ++i) {
        string sym = g->get_symbol_str(i);
        if (sym.empty()) break;
        if (!g->is_non_terminal(i)) continue;
        
        ss << "First(" << sym << ") = { ";
        const auto& f_set = g->get_first_set(i);
        bool first = true;
        for (int s : f_set) {
            if (!first) ss << ", ";
            ss << g->get_symbol_str(s);
            first = false;
        }
        ss << " }\n";
    }

    ss << "\n=== FOLLOW SETS ===\n";
    for (int i = 0; i < 2000; ++i) {
        string sym = g->get_symbol_str(i);
        if (sym.empty()) break;
        if (!g->is_non_terminal(i)) continue;
        
        ss << "Follow(" << sym << ") = { ";
        const auto& f_set = g->get_follow_set(i);
        bool first = true;
        for (int s : f_set) {
            if (!first) ss << ", ";
            ss << g->get_symbol_str(s);
            first = false;
        }
        ss << " }\n";
    }
    return ss.str();
}

// Helper: Format LR(1) states for output
string format_lr1_states(Grammar* g, LALRAutomata* aut) {
    // Capture stdout or redirect the output of print_lr1_states
    // Since we need to present it in a window, let's write a dedicated string-format routine
    stringstream ss;
    ss << "=== LR(1) AUTOMATA STATES ===\n";
    // Using a trick: we can read how automata prints its states, but to be completely memory-safe
    // we'll write a clean traversal here:
    // LALRAutomata doesn't expose the lr1 states vectors directly in a public string converter, 
    // but wait! We added "print_lr1_states" in lalr_automata.h. To write it to string:
    // Since it goes to cout, we could temporarily capture cout or just rewrite the traversal here.
    // Let's rewrite a clean, concise traversal of the states for extreme elegance!
    // But wait! `lr1_states` is private in `LALRAutomata`.
    // Let's modify LALRAutomata to return these strings, or capture standard output,
    // or wait! We can just call a command line version, but wait, a direct C++ function is much cleaner.
    // Let's redirect standard cout to stringstream!
    
    streambuf* old_cout = cout.rdbuf(); // save original rdbuf
    stringstream capturer;
    cout.rdbuf(capturer.rdbuf()); // redirect cout to our stringstream
    
    aut->print_lr1_states();
    
    cout.rdbuf(old_cout); // restore original rdbuf
    return capturer.str();
}

// Helper: Format LALR(1) states for output
string format_lalr_states(LALRAutomata* aut) {
    streambuf* old_cout = cout.rdbuf();
    stringstream capturer;
    cout.rdbuf(capturer.rdbuf());
    
    aut->print_lalr_states();
    
    cout.rdbuf(old_cout);
    return capturer.str();
}

// Helper: Format LALR(1) Parsing Table
string format_lalr_table(LALRAutomata* aut) {
    streambuf* old_cout = cout.rdbuf();
    stringstream capturer;
    cout.rdbuf(capturer.rdbuf());
    
    aut->print_parsing_table();
    
    cout.rdbuf(old_cout);
    return capturer.str();
}

bool initialize_parser(HWND hwnd) {
    // Write the BNF editor text to a temporary file
    string bnf_content = get_edit_text(hEditBNF);
    if (bnf_content.empty()) {
        MessageBoxA(hwnd, "Please load or write a BNF grammar first!", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    ofstream temp_bnf("temp_grammar.txt");
    temp_bnf << bnf_content;
    temp_bnf.close();

    if (current_grammar) delete current_grammar;
    if (current_automata) delete current_automata;

    current_grammar = new Grammar();
    if (!current_grammar->load_from_file("temp_grammar.txt")) {
        MessageBoxA(hwnd, "Failed to parse BNF Grammar rules! Check formatting.", "Grammar Parsing Error", MB_OK | MB_ICONERROR);
        return false;
    }

    current_automata = new LALRAutomata(current_grammar);
    current_automata->generate();

    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

            // --- COLUMN 1: BNF GRAMMAR INPUT (Width: 380px) ---
            CreateWindowA("STATIC", "BNF Grammar Rules (Input):", WS_VISIBLE | WS_CHILD, 10, 10, 300, 20, hwnd, NULL, NULL, NULL);
            hEditBNF = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                10, 30, 380, 520, hwnd, NULL, NULL, NULL);

            int btn_y = 560;
            CreateWindowA("BUTTON", "Load BNF File", WS_VISIBLE | WS_CHILD, 10, btn_y, 110, 35, hwnd, (HMENU)ID_BTN_LOAD_BNF, NULL, NULL);
            CreateWindowA("BUTTON", "First/Follow Sets", WS_VISIBLE | WS_CHILD, 130, btn_y, 120, 35, hwnd, (HMENU)ID_BTN_CALC_FF, NULL, NULL);
            CreateWindowA("BUTTON", "View LALR Table", WS_VISIBLE | WS_CHILD, 260, btn_y, 130, 35, hwnd, (HMENU)ID_BTN_LALR_TABLE, NULL, NULL);
            
            btn_y += 45;
            CreateWindowA("BUTTON", "View LR(1) DFA", WS_VISIBLE | WS_CHILD, 10, btn_y, 180, 35, hwnd, (HMENU)ID_BTN_LR1_DFA, NULL, NULL);
            CreateWindowA("BUTTON", "View LALR(1) DFA", WS_VISIBLE | WS_CHILD, 210, btn_y, 180, 35, hwnd, (HMENU)ID_BTN_LALR_DFA, NULL, NULL);

            // --- COLUMN 2: TOKEN MAPPING & LEX INPUT (Width: 300px) ---
            CreateWindowA("STATIC", "Token ID Map (Customize):", WS_VISIBLE | WS_CHILD, 410, 10, 250, 20, hwnd, NULL, NULL, NULL);
            hEditTokenMap = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                410, 30, 300, 200, hwnd, NULL, NULL, NULL);

            CreateWindowA("BUTTON", "Reset Map to TINY", WS_VISIBLE | WS_CHILD, 410, 240, 140, 30, hwnd, (HMENU)ID_BTN_RESET_MAP, NULL, NULL);
            CreateWindowA("BUTTON", "Import Regex Map", WS_VISIBLE | WS_CHILD, 560, 240, 150, 30, hwnd, (HMENU)ID_BTN_IMPORT_REGEX, NULL, NULL);

            CreateWindowA("STATIC", "Source Token Stream (.lex):", WS_VISIBLE | WS_CHILD, 410, 280, 250, 20, hwnd, NULL, NULL, NULL);
            hEditLex = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                410, 300, 300, 250, hwnd, NULL, NULL, NULL);

            CreateWindowA("BUTTON", "Load .lex File", WS_VISIBLE | WS_CHILD, 410, 560, 120, 40, hwnd, (HMENU)ID_BTN_LOAD_LEX, NULL, NULL);
            CreateWindowA("BUTTON", "Run LALR Parser", WS_VISIBLE | WS_CHILD, 540, 560, 170, 40, hwnd, (HMENU)ID_BTN_RUN_PARSER, NULL, NULL);

            // --- COLUMN 3: GLOBAL OUTPUT CONSOLE (Width: 630px) ---
            CreateWindowA("STATIC", "Console / Result Presentation Panel:", WS_VISIBLE | WS_CHILD, 730, 10, 350, 20, hwnd, NULL, NULL, NULL);
            hEditOut = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                730, 30, 630, 610, hwnd, NULL, NULL, NULL);

            // Set uniform styling fonts
            SendMessage(hEditBNF, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditTokenMap, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditLex, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditOut, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Set default Token map in text control
            if (current_engine) delete current_engine;
            // Create dummy grammar/automata to extract default mapping
            Grammar g; LALRAutomata aut(&g);
            current_engine = new ParserEngine(&g, &aut);
            set_edit_text(hEditTokenMap, current_engine->get_token_map_string());

            // Load default TINY grammar to make first launch gorgeous
            string default_tiny_grammar = 
                "program -> stmt-sequence\n"
                "stmt-sequence -> stmt-sequence ; statement | statement\n"
                "statement -> if-stmt | repeat-stmt | assign-stmt | read-stmt | write-stmt\n"
                "if-stmt -> if exp then stmt-sequence end | if exp then stmt-sequence else stmt-sequence end\n"
                "repeat-stmt -> repeat stmt-sequence until exp\n"
                "assign-stmt -> identifier := exp\n"
                "read-stmt -> read identifier\n"
                "write-stmt -> write exp\n"
                "exp -> simple-exp comparison-op simple-exp | simple-exp\n"
                "comparison-op -> < | > | = | <= | <> | >=\n"
                "simple-exp -> simple-exp addop term | term\n"
                "addop -> + | -\n"
                "term -> term mulop power | power\n"
                "mulop -> * | /\n"
                "power -> factor\n" // simplified or powered standard
                "factor -> ( exp ) | number | identifier\n";
            set_edit_text(hEditBNF, default_tiny_grammar);
            set_edit_text(hEditOut, "Welcome to ParserGen LALR(1) Parser Generator!\nLoad a BNF grammar file or write one, then run analysis.");

            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_BTN_LOAD_BNF: {
                    string filePath = open_file_dialog(hwnd, "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
                    if (!filePath.empty()) {
                        current_bnf_path = filePath;
                        string content = read_file(filePath);
                        set_edit_text(hEditBNF, content);
                        set_edit_text(hEditOut, "Loaded grammar from: " + filePath + "\nClick 'First/Follow Sets' or DFA buttons to process.");
                    }
                    break;
                }

                case ID_BTN_RESET_MAP: {
                    if (current_engine) delete current_engine;
                    Grammar g; LALRAutomata aut(&g);
                    current_engine = new ParserEngine(&g, &aut);
                    set_edit_text(hEditTokenMap, current_engine->get_token_map_string());
                    set_edit_text(hEditOut, "Token ID map reset to default TINY compiler encoding.");
                    break;
                }

                case ID_BTN_IMPORT_REGEX: {
                    string filePath = open_file_dialog(hwnd, "Regex Definitions (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
                    if (!filePath.empty()) {
                        string content = read_file(filePath);
                        map<int, string> imported = import_token_map_from_regex(content);
                        
                        stringstream map_ss;
                        for (auto const& kv : imported) {
                            map_ss << kv.first << " " << kv.second << "\n";
                        }
                        
                        set_edit_text(hEditTokenMap, map_ss.str());
                        
                        stringstream msg_ss;
                        msg_ss << "Successfully imported " << imported.size() << " Token ID mapping(s) from:\n" << filePath;
                        set_edit_text(hEditOut, msg_ss.str());
                    }
                    break;
                }

                case ID_BTN_CALC_FF: {
                    if (!initialize_parser(hwnd)) break;
                    set_edit_text(hEditOut, format_first_follow(current_grammar));
                    break;
                }

                case ID_BTN_LR1_DFA: {
                    if (!initialize_parser(hwnd)) break;
                    set_edit_text(hEditOut, "Constructing LR(1) states...\n" + format_lr1_states(current_grammar, current_automata));
                    break;
                }

                case ID_BTN_LALR_DFA: {
                    if (!initialize_parser(hwnd)) break;
                    set_edit_text(hEditOut, "Constructing LALR(1) states (grouped LR1 cores)...\n" + format_lalr_states(current_automata));
                    break;
                }

                case ID_BTN_LALR_TABLE: {
                    if (!initialize_parser(hwnd)) break;
                    string res = format_lalr_table(current_automata);
                    if (current_automata->has_conflicts()) {
                        res = "!!! WARNING: Grammatical conflicts (Shift/Reduce or Reduce/Reduce) detected in LALR(1) parsing table!\n\n" + res;
                    }
                    set_edit_text(hEditOut, res);
                    break;
                }

                case ID_BTN_LOAD_LEX: {
                    string filePath = open_file_dialog(hwnd, "Lex Files (*.lex)\0*.lex\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
                    if (!filePath.empty()) {
                        current_lex_path = filePath;
                        string content = read_file(filePath);
                        set_edit_text(hEditLex, content);
                        set_edit_text(hEditOut, "Loaded Lexer token stream: " + filePath + "\nClick 'Run LALR Parser' to evaluate.");
                    }
                    break;
                }

                case ID_BTN_RUN_PARSER: {
                    if (!initialize_parser(hwnd)) break;

                    string lex_input = get_edit_text(hEditLex);
                    if (lex_input.empty()) {
                        MessageBoxA(hwnd, "Please load or input a Token stream (.lex) to parse!", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }

                    string token_map_str = get_edit_text(hEditTokenMap);
                    
                    if (current_engine) delete current_engine;
                    current_engine = new ParserEngine(current_grammar, current_automata);
                    current_engine->load_token_map_from_string(token_map_str);

                    bool success = current_engine->parse(lex_input);

                    stringstream ss;
                    if (success) {
                        ss << "=========================================================\n";
                        ss << "           LALR(1) PARSING SUCCESSFUL!                   \n";
                        ss << "=========================================================\n\n";
                        ss << "=== SYNTAX TREE REPRESENTATION ===\n";
                        ss << current_engine->get_syntax_tree_string() << "\n\n";
                    } else {
                        ss << "=========================================================\n";
                        ss << "           LALR(1) PARSING FAILED (SYNTAX ERROR)         \n";
                        ss << "=========================================================\n\n";
                    }

                    ss << "=== DETAILED STEP-BY-STEP PARSING PROCESS ===\n";
                    ss << current_engine->get_parse_log() << "\n";

                    set_edit_text(hEditOut, ss.str());
                    break;
                }
            }
            break;
        }

        case WM_DESTROY:
            if (current_grammar) delete current_grammar;
            if (current_automata) delete current_automata;
            if (current_engine) delete current_engine;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char* className = "ParserGenApp";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowExA(
        0, className, "LALR(1) Parser Generator (ParserGen)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, // prevent resize complexity
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 700,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
