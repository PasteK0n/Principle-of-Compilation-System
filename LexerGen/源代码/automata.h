#ifndef AUTOMATA_H
#define AUTOMATA_H

#include <vector>
#include <string>
#include <set>
#include <map>
#include <iostream>

const int EPSILON = 256;

// NFA state and transitions
struct NFAState {
    int id;
    std::vector<std::pair<int, int>> transitions; // char/EPSILON -> state_id
    bool is_accept;
    int token_id; 
    std::string token_name;

    NFAState(int i) : id(i), is_accept(false), token_id(-1) {}
    void add_transition(int c, int to_state) {
        transitions.push_back({c, to_state});
    }
};

struct NFA {
    std::vector<NFAState*> states;
    int start_state;
    int end_state; // usually only one accepting state during regex construction
    
    NFA() : start_state(-1), end_state(-1) {}
    ~NFA() {
        for (auto s : states) delete s;
    }
    NFAState* new_state() {
        NFAState* s = new NFAState(states.size());
        states.push_back(s);
        return s;
    }
    
    std::string to_string() const;
};

// DFA state
struct DFAState {
    int id;
    std::set<int> nfa_states; // subset of NFA states
    std::map<int, int> transitions; // char -> state_id
    bool is_accept;
    int token_id;
    std::string token_name;

    DFAState(int i) : id(i), is_accept(false), token_id(-1) {}
    void add_transition(int c, int to_state) {
        transitions[c] = to_state;
    }
};

struct DFA {
    std::vector<DFAState*> states;
    int start_state;
    std::vector<int> accept_states;

    DFA() : start_state(-1) {}
    ~DFA() {
        for (auto s : states) delete s;
    }
    DFAState* new_state() {
        DFAState* s = new DFAState(states.size());
        states.push_back(s);
        return s;
    }

    std::string to_string() const;
};

// Conversions
DFA* nfa_to_dfa(NFA* nfa);
DFA* minimize_dfa(DFA* dfa);

#endif // AUTOMATA_H
