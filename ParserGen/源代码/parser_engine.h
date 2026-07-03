#ifndef PARSER_ENGINE_H
#define PARSER_ENGINE_H

#include "grammar.h"
#include "lalr_automata.h"
#include <string>
#include <vector>
#include <map>
#include <stack>

struct SyntaxTreeNode {
    int id;
    std::string name;
    std::string lexeme;
    std::vector<SyntaxTreeNode*> children;

    SyntaxTreeNode(int node_id, const std::string& node_name, const std::string& node_lexeme = "")
        : id(node_id), name(node_name), lexeme(node_lexeme) {}
    
    ~SyntaxTreeNode() {
        for (auto child : children) {
            delete child;
        }
    }
};

struct TokenInstance {
    int token_id;
    std::string lexeme;
    std::string grammar_symbol; // mapped grammar terminal
};

struct ParseStep {
    int step_number;
    std::vector<int> state_stack;
    std::vector<std::string> symbol_stack;
    std::vector<TokenInstance> remaining_input;
    std::string action_desc;
};

class ParserEngine {
public:
    ParserEngine(Grammar* g, LALRAutomata* aut);
    ~ParserEngine();

    // Sets up the token-to-grammar-symbol mapping
    void set_token_map(const std::map<int, std::string>& mapping);
    void load_token_map_from_string(const std::string& map_str);
    std::string get_token_map_string() const;

    // Parse the token stream
    bool parse(const std::string& lex_file_content);

    // Get parsing process details
    const std::vector<ParseStep>& get_parse_steps() const { return parse_steps; }
    SyntaxTreeNode* get_syntax_tree() const { return syntax_tree_root; }

    // Print syntax tree as structured text
    std::string get_syntax_tree_string() const;
    void print_syntax_tree(SyntaxTreeNode* node, int depth, std::stringstream& ss) const;

    std::string get_parse_log() const;

private:
    Grammar* grammar;
    LALRAutomata* automata;
    std::map<int, std::string> token_map;

    std::vector<ParseStep> parse_steps;
    SyntaxTreeNode* syntax_tree_root;
    int next_node_id;

    std::vector<TokenInstance> tokenize_lex_file(const std::string& content);
    void clean_tree();
};

#endif // PARSER_ENGINE_H
