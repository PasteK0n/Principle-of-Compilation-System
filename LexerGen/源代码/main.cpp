/*******************************************************************************
 * (1) 版权信息 
 * Copyright (c) 2026 [South China Normal University]. All rights reserved.
 *
 * (2) 文件名，标识符，摘要或模块功能说明
 * @file        main.cpp
 * @brief       用于实现可视化窗口以及核心交互逻辑的主程序模块，集成了正则表达式解析、自动机构建、代码生成和测试功能
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

#include <windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include "regex_parser.h"
#include "automata.h"
#include "lexer_gen.h"

using namespace std;

#define ID_BTN_LOAD 101
#define ID_BTN_NFA 102
#define ID_BTN_DFA 103
#define ID_BTN_MINDFA 104
#define ID_BTN_GEN 105
#define ID_BTN_TEST 106
#define ID_BTN_SAVE_DEF 107
#define ID_BTN_LOAD_TEST 108

HWND hEditIn, hEditOut;
string current_test_file = "sample.tny"; // default
LexerDef current_def;
DFA* current_dfa = nullptr;
DFA* current_mindfa = nullptr;

string get_edit_text(HWND hwnd) {
    int len = GetWindowTextLengthA(hwnd);
    if(len == 0) return "";
    char* buf = new char[len + 1];
    GetWindowTextA(hwnd, buf, len + 1);
    string s = buf;
    delete[] buf;
    return s;
}

void set_edit_text(HWND hwnd, const string& text) {
    string win_text;
    for(size_t i=0; i<text.length(); i++) {
        if(text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            win_text += "\r\n";
        } else {
            win_text += text[i];
        }
    }
    SetWindowTextA(hwnd, win_text.c_str());
}

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

string read_file(const string& path) {
    ifstream in(path);
    if(!in) return "";
    return string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

            CreateWindowA("STATIC", "Regex Definitions:", WS_VISIBLE | WS_CHILD, 10, 10, 200, 20, hwnd, NULL, NULL, NULL);
            hEditIn = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                10, 30, 380, 500, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "Output:", WS_VISIBLE | WS_CHILD, 530, 10, 200, 20, hwnd, NULL, NULL, NULL);
            hEditOut = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                530, 30, 650, 500, hwnd, NULL, NULL, NULL);

            SendMessage(hEditIn, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditOut, WM_SETFONT, (WPARAM)hFont, TRUE);

            int btn_x = 400;
            int btn_y = 30;
            CreateWindowA("BUTTON", "Load Regex definitions", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 30, hwnd, (HMENU)ID_BTN_LOAD, NULL, NULL); btn_y += 40;
            CreateWindowA("BUTTON", "Build NFA", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 30, hwnd, (HMENU)ID_BTN_NFA, NULL, NULL); btn_y += 40;
            CreateWindowA("BUTTON", "Build DFA", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 30, hwnd, (HMENU)ID_BTN_DFA, NULL, NULL); btn_y += 40;
            CreateWindowA("BUTTON", "Min DFA", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 30, hwnd, (HMENU)ID_BTN_MINDFA, NULL, NULL); btn_y += 40;
            CreateWindowA("BUTTON", "Gen Lexer Code", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 30, hwnd, (HMENU)ID_BTN_GEN, NULL, NULL); btn_y += 40;
            
            CreateWindowA("STATIC", "Testing:", WS_VISIBLE | WS_CHILD, btn_x, btn_y + 10, 120, 20, hwnd, NULL, NULL, NULL); btn_y += 30;
            CreateWindowA("BUTTON", "Load Source File", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 30, hwnd, (HMENU)ID_BTN_LOAD_TEST, NULL, NULL); btn_y += 40;
            CreateWindowA("BUTTON", "Compile & Run", WS_VISIBLE | WS_CHILD, btn_x, btn_y, 120, 40, hwnd, (HMENU)ID_BTN_TEST, NULL, NULL); btn_y += 50;
            
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch(wmId) {
                case ID_BTN_LOAD: {
                    string filePath = open_file_dialog(hwnd, "Text Files\0*.txt\0All Files\0*.*\0");
                    if(!filePath.empty()) {
                        string content = read_file(filePath);
                        set_edit_text(hEditIn, content);
                        set_edit_text(hEditOut, "Loaded " + filePath);
                    }
                    break;
                }
                case ID_BTN_LOAD_TEST: {
                    string filePath = open_file_dialog(hwnd, "TINY Source (*.tny)\0*.tny\0C Source (*.c)\0*.c\0All Files\0*.*\0");
                    if(!filePath.empty()) {
                        current_test_file = filePath;
                        set_edit_text(hEditOut, "Test source selected: " + current_test_file + "\nUse 'Compile & Run' to generate tokens.");
                    }
                    break;
                }
                case ID_BTN_NFA: {
                    string text = get_edit_text(hEditIn);
                    if(text.empty()) { MessageBoxA(hwnd, "Please input regular expressions", "Error", MB_OK); break; }
                    
                    if(current_def.global_nfa) delete current_def.global_nfa;
                    current_def = parse_lexer_definitions(text);
                    set_edit_text(hEditOut, "NFA constructed successfully!\n\n" + current_def.global_nfa->to_string());
                    break;
                }
                case ID_BTN_DFA: {
                    if(!current_def.global_nfa) { MessageBoxA(hwnd, "Build NFA first", "Error", MB_OK); break; }
                    if(current_dfa) delete current_dfa;
                    current_dfa = nfa_to_dfa(current_def.global_nfa);
                    set_edit_text(hEditOut, "DFA constructed successfully!\n\n" + current_dfa->to_string());
                    break;
                }
                case ID_BTN_MINDFA: {
                    if(!current_dfa) { MessageBoxA(hwnd, "Build DFA first", "Error", MB_OK); break; }
                    if(current_mindfa) delete current_mindfa;
                    current_mindfa = minimize_dfa(current_dfa);
                    set_edit_text(hEditOut, "Minimized DFA constructed successfully!\n\n" + current_mindfa->to_string());
                    break;
                }
                case ID_BTN_GEN: {
                    if(!current_mindfa) { MessageBoxA(hwnd, "Build Min DFA first", "Error", MB_OK); break; }
                    string code = generate_lexer_cpp(current_mindfa, current_def.keywords);
                    
                    ofstream out("lexer.cpp");
                    out << code;
                    out.close();
                    
                    set_edit_text(hEditOut, "Lexer Generator code written to lexer.cpp.\n\n" + code);
                    break;
                }
                case ID_BTN_TEST: {
                    // Compile lexer.cpp and run it
                    set_edit_text(hEditOut, "Compiling lexer.cpp...");
                    
                    int ret = system("g++ -O2 lexer.cpp -o lexer.exe");
                    if(ret != 0) {
                        set_edit_text(hEditOut, "Compilation failed! (Is g++ in PATH?)");
                        break;
                    }
                    
                    if(current_test_file.empty()) {
                        MessageBoxA(hwnd, "No test file specified", "Error", MB_OK);
                        break;
                    }
                    
                    string cmd = "lexer.exe \"" + current_test_file + "\" > temp_lex_output.txt 2>&1";
                    system(cmd.c_str());
                    
                    string lextxt = read_file("temp_lex_output.txt");
                    
                    // also read the generated .lex file directly
                    string lex_filename = current_test_file;
                    size_t p = lex_filename.find_last_of('.');
                    if(p != string::npos) lex_filename = lex_filename.substr(0, p);
                    lex_filename += ".lex";
                    
                    string saved_lex = read_file(lex_filename);
                    
                    set_edit_text(hEditOut, "=== Execution Standard Output ===\n" + lextxt + 
                                           "\n\n=== Saved Lexer Result (" + lex_filename + ") ===\n" + saved_lex);
                    break;
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char* className = "LexerGenApp";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if(!RegisterClass(&wc)) {
        MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowExA(
        0, className, "Lexical Analyzer Generator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 600,
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL) {
        MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
