#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

const int EPSILON_ID = 0;
const int END_MARKER_ID = 1; // Used for end of input i.e. $

/*
 * Represents a single grammar production rule:
 * lhs -> rhs[0] rhs[1] ... rhs[n-1]
 */
struct Production {
    int id; // unique ID for the production
    int lhs; // symbol ID for the left-hand side
    std::vector<int> rhs; // symbol IDs for the right-hand side
};

class Grammar {
public:
    Grammar();
    ~Grammar();

    // Parse BNF from text file. Returns true if successful.
    bool load_from_file(const std::string& filepath);

    // Get symbol ID from string. If it doesn't exist, create it if create_if_missing is true.
    int get_symbol_id(const std::string& sym, bool create_if_missing = true);
    
    // Get symbol string from ID
    std::string get_symbol_str(int id) const;

    // Returns true if the symbol ID represents a non-terminal
    bool is_non_terminal(int id) const;

    // First and Follow set accessors
    const std::set<int>& get_first_set(int symbol_id) const;
    const std::set<int>& get_follow_set(int symbol_id) const;

    // Get First set of a sequence of symbols (useful for LR items)
    std::set<int> compute_first_of_sequence(const std::vector<int>& seq) const;

    // Grammar data
    std::vector<Production> productions;
    int start_symbol_id;

    // For debugging/output
    void print_grammar() const;
    void print_first_sets() const;
    void print_follow_sets() const;

private:
    std::vector<std::string> id_to_symbol;
    std::unordered_map<std::string, int> symbol_to_id;
    std::unordered_set<int> non_terminals;

    std::unordered_map<int, std::set<int>> first_sets;
    std::unordered_map<int, std::set<int>> follow_sets;

    void compute_first_sets();
    void compute_follow_sets();
    
    // Helper to tokenize a line
    std::vector<std::string> tokenize(const std::string& line);
};

#endif // GRAMMAR_H
