#ifndef LALR_AUTOMATA_H
#define LALR_AUTOMATA_H

#include "grammar.h"
#include <set>
#include <vector>
#include <map>

// Represents an LR(1) item: [A -> alpha . beta, a]
struct LRItem {
    int prod_id;   // production index
    int dot_pos;   // position of dot, 0 to rhs.size()
    int lookahead; // lookahead symbol

    bool operator<(const LRItem& other) const {
        if (prod_id != other.prod_id) return prod_id < other.prod_id;
        if (dot_pos != other.dot_pos) return dot_pos < other.dot_pos;
        return lookahead < other.lookahead;
    }
    bool operator==(const LRItem& other) const {
        return prod_id == other.prod_id && dot_pos == other.dot_pos && lookahead == other.lookahead;
    }
};

struct LRState {
    int id;
    std::set<LRItem> items;
    std::map<int, int> transitions; // symbol_id -> next_state_id
};

enum ActionType {
    ACTION_ERROR,
    ACTION_SHIFT,
    ACTION_REDUCE,
    ACTION_ACCEPT
};

struct Action {
    ActionType type = ACTION_ERROR;
    int target = -1; // state id for shift, prod id for reduce
};

class LALRAutomata {
public:
    LALRAutomata(Grammar* g);
    ~LALRAutomata();

    void generate();

    // Table outputs
    void print_lr1_states() const;
    void print_lalr_states() const;
    void print_parsing_table() const;
    
    bool has_conflicts() const { return conflicts_exist; }

    // Export Action/Goto map for external parser engine
    std::vector<std::map<int, Action>> action_table;
    std::vector<std::map<int, int>> goto_table;

private:
    Grammar* grammar;

    // LR(1) data
    std::vector<LRState> lr1_states;
    std::map<std::set<LRItem>, int> lr1_state_map;

    // LALR(1) data
    std::vector<LRState> lalr_states;

    bool conflicts_exist;

    std::set<LRItem> closure(const std::set<LRItem>& items) const;
    std::set<LRItem> goto_state(const std::set<LRItem>& items, int symbol) const;
    void build_lr1();
    void build_lalr();
    void build_tables();
    
    // Core of an item ignores the lookahead
    struct ItemCore {
        int prod_id;
        int dot_pos;
        bool operator<(const ItemCore& other) const {
            if (prod_id != other.prod_id) return prod_id < other.prod_id;
            return dot_pos < other.dot_pos;
        }
    };
    std::set<ItemCore> get_core(const std::set<LRItem>& items) const;
};

#endif // LALR_AUTOMATA_H
