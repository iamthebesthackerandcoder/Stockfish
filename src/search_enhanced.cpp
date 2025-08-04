/*
  Stockfish Enhanced Search Implementation
  Advanced search algorithms with improved pruning, move ordering, and evaluation
*/

#include "search_enhanced.h"
#include "evaluate.h"
#include "movegen.h"
#include "movepick.h"
#include "position.h"
#include "tt.h"
#include "uci.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace Stockfish {

namespace SearchEnhanced {

// Enhanced History Implementation
void EnhancedHistory::update_killer_moves(Move move, int ply) {
    if (ply >= MAX_PLY) return;
    
    if (killer_moves[ply][0] != move) {
        killer_moves[ply][1] = killer_moves[ply][0];
        killer_moves[ply][0] = move;
    }
}

void EnhancedHistory::update_history_heuristic(Move move, Color color, int depth, bool failed_high) {
    int bonus = failed_high ? depth * depth : -depth * depth / 4;
    int& entry = history_table[color][move.from_sq()][move.to_sq()];
    entry += bonus - entry * std::abs(bonus) / 16384;
}

void EnhancedHistory::update_butterfly_history(Move move, Color color, int bonus) {
    int& entry = butterfly_table[color][move.from_sq()][move.to_sq()];
    entry += bonus - entry * std::abs(bonus) / 16384;
}

void EnhancedHistory::clear() {
    std::memset(killer_moves, 0, sizeof(killer_moves));
    std::memset(history_table, 0, sizeof(history_table));
    std::memset(butterfly_table, 0, sizeof(butterfly_table));
    std::memset(counter_moves, 0, sizeof(counter_moves));
}

Move EnhancedHistory::get_killer_move(int ply, int index) const {
    return (ply < MAX_PLY && index < 2) ? killer_moves[ply][index] : Move::none();
}

int EnhancedHistory::get_history_score(Move move, Color color) const {
    return history_table[color][move.from_sq()][move.to_sq()];
}

int EnhancedHistory::get_butterfly_score(Move move, Color color) const {
    return butterfly_table[color][move.from_sq()][move.to_sq()];
}

// Enhanced Transposition Table Implementation
void EnhancedTT::store(uint64_t key, Value value, Value eval, Move move, 
                      Depth depth, Bound bound, int ply) {
    if (!table) return;
    
    uint64_t idx = key & (size - 1);
    Cluster& cluster = table[idx];
    
    TTEntry* entry = get_replacement_entry(cluster, key);
    
    // Don't overwrite more valuable entries
    if (entry->key != key && entry->depth > depth - 4 && 
        entry->generation == generation) {
        return;
    }
    
    entry->key = key;
    entry->value = value;
    entry->eval = eval;
    entry->best_move = move;
    entry->depth = depth;
    entry->bound = bound;
    entry->generation = generation;
    entry->age_bonus = 0;
}

bool EnhancedTT::probe(uint64_t key, TTEntry& entry) const {
    if (!table) return false;
    
    uint64_t idx = key & (size - 1);
    const Cluster& cluster = table[idx];
    
    for (const auto& e : cluster.entries) {
        if (e.key == key) {
            entry = e;
            return true;
        }
    }
    return false;
}

void EnhancedTT::new_search() {
    generation++;
}

void EnhancedTT::clear() {
    if (table) {
        std::fill_n(reinterpret_cast<char*>(table.get()), 
                   size * sizeof(Cluster), 0);
    }
}

EnhancedTT::TTEntry* EnhancedTT::get_replacement_entry(Cluster& cluster, uint64_t key) const {
    TTEntry* best = &cluster.entries[0];
    
    for (auto& entry : cluster.entries) {
        if (entry.key == key) {
            return &entry;
        }
        
        // Prefer entries with older generation or lower depth
        if ((entry.generation != generation && best->generation == generation) ||
            (entry.generation == best->generation && entry.depth < best->depth)) {
            best = &entry;
        }
    }
    
    return best;
}

// Enhanced Worker Implementation
EnhancedWorker::EnhancedWorker(Search::SharedState& shared, 
                              std::unique_ptr<Search::ISearchManager> manager, 
                              size_t threadIdx, NumaReplicatedAccessToken token)
    : Search::Worker(shared, std::move(manager), threadIdx, token), search_age(0) {
    
    enhanced_history.clear();
    enhanced_tt.clear();
    stats = {};
}

template<NodeType nodeType>
Value EnhancedWorker::enhanced_search(Position& pos, Search::Stack* ss, Value alpha, Value beta, 
                                     Depth depth, bool cutNode) {
    constexpr bool PvNode = (nodeType != NonPV);
    constexpr bool rootNode = (nodeType == Root);
    
    // Initialize search statistics
    stats.nodes_searched++;
    
    // Check for immediate returns
    if (depth <= 0) {
        return enhanced_qsearch(pos, ss, alpha, beta);
    }
    
    // Mate distance pruning
    if (!rootNode) {
        alpha = std::max(mated_in(ss->ply), alpha);
        beta = std::min(mate_in(ss->ply + 1), beta);
        if (alpha >= beta) {
            return alpha;
        }
    }
    
    // Transposition table lookup
    const uint64_t posKey = pos.key();
    EnhancedTT::TTEntry ttEntry;
    bool ttHit = enhanced_tt.probe(posKey, ttEntry);
    Move ttMove = ttHit ? ttEntry.best_move : Move::none();
    Value ttValue = ttHit ? ttEntry.value : VALUE_NONE;
    
    // TT cutoff
    if (!PvNode && ttHit && ttEntry.depth >= depth && 
        ((ttEntry.bound & BOUND_LOWER && ttValue >= beta) ||
         (ttEntry.bound & BOUND_UPPER && ttValue <= alpha) ||
         (ttEntry.bound == BOUND_EXACT))) {
        return ttValue;
    }
    
    // Evaluation and static analysis
    bool inCheck = pos.checkers();
    Value eval = inCheck ? VALUE_NONE : evaluate(pos);
    
    // Evaluation cache for complex positions
    if (!inCheck && eval_cache.find(posKey) != eval_cache.end()) {
        auto& cache_entry = eval_cache[posKey];
        if (cache_entry.age == search_age) {
            eval = cache_entry.eval;
        }
    }
    
    ss->staticEval = eval;
    bool improving = !inCheck && ss->staticEval > (ss-2)->staticEval;
    
    // Enhanced Razoring
    if (!PvNode && !inCheck && depth < 3 && 
        razoring_cutoff(pos, alpha, depth, eval)) {
        return enhanced_qsearch(pos, ss, alpha, beta);
    }
    
    // Enhanced Futility Pruning
    if (!PvNode && !inCheck && depth < 8 && 
        futility_pruning(pos, alpha, depth, eval, improving)) {
        return eval;
    }
    
    // Null move pruning with adaptive depth reduction
    if (!PvNode && !inCheck && eval >= beta && 
        pos.non_pawn_material(pos.side_to_move()) && depth >= PruningParams::ADAPTIVE_NULL_MOVE_DEPTH) {
        
        Depth R = 3 + depth / 4 + std::min(3, (eval - beta) / 200);
        
        ss->currentMove = Move::null();
        do_null_move(pos, *(++ss));
        
        Value nullValue = -enhanced_search<NonPV>(pos, ss, -beta, -beta + 1, depth - R, !cutNode);
        
        undo_null_move(pos);
        ss--;
        
        if (nullValue >= beta) {
            stats.null_move_cutoffs++;
            return nullValue;
        }
    }
    
    // Internal iterative deepening for nodes without TT move
    if (PvNode && depth >= 6 && !ttMove) {
        enhanced_search<PV>(pos, ss, alpha, beta, depth - 4, cutNode);
        
        // Re-probe TT after IID
        if (enhanced_tt.probe(posKey, ttEntry)) {
            ttMove = ttEntry.best_move;
        }
    }
    
    // Move generation and ordering
    MovePicker mp(pos, ttMove, depth, &mainHistory, &lowPlyHistory, 
                  &captureHistory, nullptr, &pawnHistory, ss->ply);
    
    Value bestValue = -VALUE_INFINITE;
    Value value;
    Move bestMove = Move::none();
    int moveCount = 0;
    int quietCount = 0;
    bool first_move = true;
    
    // Enhanced move loop
    Move move;
    while ((move = mp.next_move()) != Move::none()) {
        if (!pos.legal(move)) continue;
        
        moveCount++;
        bool isQuiet = !pos.capture_stage(move) && move.type_of() != PROMOTION;
        if (isQuiet) quietCount++;
        
        // Late move pruning
        if (!PvNode && !inCheck && moveCount > 1 && 
            late_move_pruning(moveCount, depth, improving)) {
            break;
        }
        
        // Singular extension
        Depth extension = 0;
        if (depth >= PruningParams::SINGULAR_EXTENSION_DEPTH && move == ttMove && 
            !rootNode && should_extend_singular(pos, move, beta, depth)) {
            extension = 1;
        }
        
        // Calculate reduction
        Depth reduction = 0;
        if (depth >= 3 && moveCount > 1) {
            int history_score = enhanced_history.get_history_score(move, pos.side_to_move());
            reduction = calculate_reduction(PvNode, improving, depth, moveCount, 
                                          !isQuiet, history_score);
        }
        
        // Make move
        do_move(pos, move, *(++ss));
        
        // Principal Variation Search
        if (first_move) {
            value = -enhanced_search<nodeType>(pos, ss, -beta, -alpha, depth - 1 + extension, false);
            first_move = false;
        } else {
            // Late Move Reduction
            value = -enhanced_search<NonPV>(pos, ss, -alpha - 1, -alpha, 
                                          depth - 1 - reduction + extension, true);
            
            // Full search if LMR failed high or for PV nodes
            if (value > alpha && (reduction > 0 || PvNode)) {
                value = -enhanced_search<nodeType>(pos, ss, -beta, -alpha, 
                                                 depth - 1 + extension, false);
            }
        }
        
        // Undo move
        undo_move(pos, move);
        ss--;
        
        // Update best value and alpha
        if (value > bestValue) {
            bestValue = value;
            bestMove = move;
            
            if (value > alpha) {
                alpha = value;
                
                // Update history for good moves
                if (isQuiet && value >= beta) {
                    enhanced_history.update_killer_moves(move, ss->ply);
                    enhanced_history.update_history_heuristic(move, pos.side_to_move(), 
                                                             depth, true);
                }
                
                // Beta cutoff
                if (value >= beta) {
                    stats.beta_cutoffs++;
                    if (moveCount == 1) {
                        stats.first_move_cutoffs++;
                    }
                    update_search_stats(true, moveCount == 1);
                    break;
                }
            }
        }
        
        update_search_stats(false, false);
    }
    
    // Store in transposition table
    Bound bound = (bestValue >= beta) ? BOUND_LOWER : 
                  (bestValue <= alpha) ? BOUND_UPPER : BOUND_EXACT;
    
    enhanced_tt.store(posKey, bestValue, eval, bestMove, depth, bound, ss->ply);
    
    // Cache evaluation for complex positions
    if (!inCheck && depth >= 4) {
        eval_cache[posKey] = {posKey, eval, search_age};
    }
    
    return bestValue;
}

Value EnhancedWorker::enhanced_qsearch(Position& pos, Search::Stack* ss, Value alpha, Value beta) {
    bool inCheck = pos.checkers();
    Value bestValue = inCheck ? -VALUE_INFINITE : evaluate(pos);
    
    if (bestValue >= beta) {
        return bestValue;
    }
    
    if (bestValue > alpha) {
        alpha = bestValue;
    }
    
    MovePicker mp(pos, Move::none(), 1, nullptr, nullptr, &captureHistory, 
                  nullptr, nullptr, ss->ply);
    
    Move move;
    while ((move = mp.next_move()) != Move::none()) {
        if (!pos.legal(move)) continue;
        
        // SEE pruning in qsearch
        if (!inCheck && !pos.see_ge(move, Value(-50))) {
            continue;
        }
        
        do_move(pos, move, *(++ss));
        Value value = -enhanced_qsearch(pos, ss, -beta, -alpha);
        undo_move(pos, move);
        ss--;
        
        if (value > bestValue) {
            bestValue = value;
            if (value > alpha) {
                alpha = value;
                if (value >= beta) {
                    break;
                }
            }
        }
    }
    
    return bestValue;
}

// Enhanced pruning implementations
bool EnhancedWorker::razoring_cutoff(const Position& pos, Value alpha, Depth depth, Value eval) {
    return eval < alpha - PruningParams::RAZORING_MARGIN - 100 * depth;
}

bool EnhancedWorker::futility_pruning(const Position& pos, Value alpha, Depth depth, 
                                     Value eval, bool improving) {
    Value futility_margin = PruningParams::FUTILITY_BASE * depth;
    if (improving) futility_margin -= 50;
    
    return eval + futility_margin <= alpha;
}

bool EnhancedWorker::late_move_pruning(int moveCount, Depth depth, bool improving) {
    int threshold = PruningParams::LATE_MOVE_REDUCTION_THRESHOLD + depth * depth;
    if (improving) threshold += depth;
    
    return moveCount >= threshold;
}

Depth EnhancedWorker::calculate_reduction(bool pvNode, bool improving, Depth depth, 
                                        int moveCount, bool tactical, int history_score) {
    Depth reduction = 0;
    
    if (!tactical && moveCount > 1) {
        reduction = 1 + depth / 8 + moveCount / 16;
        
        // Reduce less for PV nodes
        if (pvNode) reduction--;
        
        // Reduce less if position is improving
        if (improving) reduction--;
        
        // Adjust based on history score
        reduction -= history_score / 8192;
        
        reduction = std::max(0, std::min(reduction, depth - 1));
    }
    
    return reduction;
}

bool EnhancedWorker::should_extend_singular(const Position& pos, Move move, 
                                           Value beta, Depth depth) {
    // Simplified singular extension logic
    return depth >= 8 && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY;
}

void EnhancedWorker::update_search_stats(bool cutoff, bool first_move) {
    // Update statistics for adaptive algorithms
    if (cutoff) {
        stats.beta_cutoffs++;
        if (first_move) stats.first_move_cutoffs++;
    }
    
    // Calculate branching factor periodically
    if (stats.nodes_searched % 10000 == 0) {
        stats.branching_factor = calculate_branching_factor();
    }
}

double EnhancedWorker::calculate_branching_factor() const {
    if (stats.beta_cutoffs == 0) return 2.0;
    
    return static_cast<double>(stats.nodes_searched) / stats.beta_cutoffs;
}

// Aspiration Search Implementation
Value AspirationSearch::search_with_aspiration(EnhancedWorker& worker, Position& pos, 
                                              Value prev_score, Depth depth) {
    Value alpha = prev_score - INITIAL_WINDOW;
    Value beta = prev_score + INITIAL_WINDOW;
    Value value;
    
    for (int iteration = 0; iteration < 10; ++iteration) {
        Search::Stack stack[MAX_PLY + 10];
        Search::Stack* ss = stack + 7;
        
        value = worker.enhanced_search<PV>(pos, ss, alpha, beta, depth, false);
        
        if (value <= alpha) {
            alpha = widen_window(alpha - prev_score, iteration);
            beta = prev_score + INITIAL_WINDOW;
        } else if (value >= beta) {
            beta = widen_window(beta - prev_score, iteration);
        } else {
            break;
        }
    }
    
    return value;
}

Value AspirationSearch::widen_window(Value window, int iteration) {
    return std::min(MAX_WINDOW, window * (2 + iteration));
}

// Multi-cut implementation
bool MultiCut::should_multi_cut(const Position& pos, Value beta, Depth depth, 
                               int moves_searched, int cutoff_count) {
    return depth >= MIN_DEPTH_FOR_MULTICUT && 
           moves_searched >= MIN_MOVES_FOR_MULTICUT &&
           cutoff_count >= MIN_CUTOFFS_FOR_MULTICUT;
}

// Explicit template instantiations
template Value EnhancedWorker::enhanced_search<NonPV>(Position& pos, Search::Stack* ss, 
                                                     Value alpha, Value beta, Depth depth, bool cutNode);
template Value EnhancedWorker::enhanced_search<PV>(Position& pos, Search::Stack* ss, 
                                                   Value alpha, Value beta, Depth depth, bool cutNode);
template Value EnhancedWorker::enhanced_search<Root>(Position& pos, Search::Stack* ss, 
                                                    Value alpha, Value beta, Depth depth, bool cutNode);

} // namespace SearchEnhanced

} // namespace Stockfish