/*
  Stockfish Enhanced Search Module
  Enhanced version with advanced search techniques, improved pruning, 
  and better move ordering algorithms.
*/

#ifndef SEARCH_ENHANCED_H_INCLUDED
#define SEARCH_ENHANCED_H_INCLUDED

#include "search.h"
#include <chrono>
#include <memory>
#include <unordered_map>

namespace Stockfish {

namespace SearchEnhanced {

// Enhanced pruning parameters
struct PruningParams {
    static constexpr int RAZORING_MARGIN = 520;
    static constexpr int FUTILITY_BASE = 100;
    static constexpr int ADAPTIVE_NULL_MOVE_DEPTH = 2;
    static constexpr int LATE_MOVE_REDUCTION_THRESHOLD = 3;
    static constexpr int ASPIRATION_WINDOW_SIZE = 15;
    static constexpr int SINGULAR_EXTENSION_DEPTH = 8;
};

// Enhanced history tracking for better move ordering
class EnhancedHistory {
public:
    void update_killer_moves(Move move, int ply);
    void update_history_heuristic(Move move, Color color, int depth, bool failed_high);
    void update_butterfly_history(Move move, Color color, int bonus);
    void clear();
    
    Move get_killer_move(int ply, int index) const;
    int get_history_score(Move move, Color color) const;
    int get_butterfly_score(Move move, Color color) const;

private:
    Move killer_moves[MAX_PLY][2];
    int history_table[COLOR_NB][SQUARE_NB][SQUARE_NB];
    int butterfly_table[COLOR_NB][SQUARE_NB][SQUARE_NB];
    int counter_moves[PIECE_TYPE_NB][SQUARE_NB];
};

// Enhanced transposition table with more sophisticated replacement schemes
class EnhancedTT {
public:
    struct TTEntry {
        uint64_t key;
        Value value;
        Value eval;
        Move best_move;
        Depth depth;
        Bound bound;
        uint8_t generation;
        int16_t age_bonus;
    };

    void store(uint64_t key, Value value, Value eval, Move move, 
               Depth depth, Bound bound, int ply);
    bool probe(uint64_t key, TTEntry& entry) const;
    void new_search();
    void clear();
    int hashfull() const;

private:
    static constexpr size_t CLUSTER_SIZE = 4;
    static constexpr size_t DEFAULT_TT_SIZE = 16; // MB
    
    struct Cluster {
        TTEntry entries[CLUSTER_SIZE];
    };
    
    std::unique_ptr<Cluster[]> table;
    size_t size;
    uint8_t generation;
    
    TTEntry* get_replacement_entry(Cluster& cluster, uint64_t key) const;
};

// Enhanced search worker with advanced algorithms
class EnhancedWorker : public Search::Worker {
public:
    EnhancedWorker(Search::SharedState& shared, std::unique_ptr<Search::ISearchManager> manager, 
                   size_t threadIdx, NumaReplicatedAccessToken token);

    // Enhanced search methods
    template<NodeType nodeType>
    Value enhanced_search(Position& pos, Search::Stack* ss, Value alpha, Value beta, 
                         Depth depth, bool cutNode);
    
    Value enhanced_qsearch(Position& pos, Search::Stack* ss, Value alpha, Value beta);
    
    // Advanced pruning techniques
    bool razoring_cutoff(const Position& pos, Value alpha, Depth depth, Value eval);
    bool futility_pruning(const Position& pos, Value alpha, Depth depth, Value eval, bool improving);
    bool late_move_pruning(int moveCount, Depth depth, bool improving);
    
    // Enhanced move ordering
    void order_moves_enhanced(MovePicker& mp, const Position& pos, Move tt_move, 
                             Search::Stack* ss, Depth depth);
    
    // Adaptive time management
    void adaptive_time_management();
    
    // Position evaluation caching
    struct EvalCacheEntry {
        uint64_t key;
        Value eval;
        uint32_t age;
    };
    
private:
    EnhancedHistory enhanced_history;
    EnhancedTT enhanced_tt;
    
    // Evaluation cache for expensive positions
    std::unordered_map<uint64_t, EvalCacheEntry> eval_cache;
    uint32_t search_age;
    
    // Search statistics for adaptive algorithms
    struct SearchStats {
        uint64_t nodes_searched;
        uint64_t beta_cutoffs;
        uint64_t first_move_cutoffs;
        uint64_t null_move_cutoffs;
        double branching_factor;
        std::chrono::steady_clock::time_point search_start;
    } stats;
    
    // Helper methods
    void update_search_stats(bool cutoff, bool first_move);
    double calculate_branching_factor() const;
    bool should_extend_singular(const Position& pos, Move move, Value beta, Depth depth);
    Depth calculate_reduction(bool pvNode, bool improving, Depth depth, int moveCount, 
                             bool tactical, int history_score);
};

// Enhanced aspiration windows search
class AspirationSearch {
public:
    Value search_with_aspiration(EnhancedWorker& worker, Position& pos, 
                                Value prev_score, Depth depth);

private:
    static constexpr Value INITIAL_WINDOW = Value(15);
    static constexpr Value MAX_WINDOW = Value(500);
    
    Value widen_window(Value window, int iteration);
};

// Multi-cut pruning implementation
class MultiCut {
public:
    static bool should_multi_cut(const Position& pos, Value beta, Depth depth, 
                                int moves_searched, int cutoff_count);

private:
    static constexpr int MIN_MOVES_FOR_MULTICUT = 6;
    static constexpr int MIN_CUTOFFS_FOR_MULTICUT = 3;
    static constexpr Depth MIN_DEPTH_FOR_MULTICUT = 3;
};

// Principal Variation Search enhancements
class PVSEnhanced {
public:
    template<NodeType nodeType>
    static Value search_pv(EnhancedWorker& worker, Position& pos, Search::Stack* ss,
                          Value alpha, Value beta, Depth depth);

private:
    static bool should_do_full_search(Value value, Value alpha, Value beta, 
                                     int moveCount, bool is_pv_node);
};

} // namespace SearchEnhanced

} // namespace Stockfish

#endif // #ifndef SEARCH_ENHANCED_H_INCLUDED