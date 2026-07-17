@echo off
cd /d "%~dp0"
echo Compiling LALR(1) Parser Generator (ParserGen)...
g++ -O3 main.cpp grammar.cpp lalr_automata.cpp parser_engine.cpp -o ParserGenerator.exe -lcomdlg32 -lgdi32 -lcomctl32
if %errorlevel% neq 0 (
    echo Compilation failed.
) else (
    echo Compilation successful! Run ParserGen\ParserGenerator.exe
)
pause
