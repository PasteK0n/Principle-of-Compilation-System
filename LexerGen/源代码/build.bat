@echo off
cd /d "%~dp0"
echo Compiling Lexical Analyzer Generator...
g++ main.cpp regex_parser.cpp automata.cpp lexer_gen.cpp -o LexerGenerator.exe -lcomdlg32 -lgdi32
if %errorlevel% neq 0 (
    echo Compilation failed.
) else (
    echo Compilation successful! Run LexerGen\LexerGenerator.exe
)
pause
