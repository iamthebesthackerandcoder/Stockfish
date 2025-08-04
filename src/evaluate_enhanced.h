/*
  Stockfish Enhanced Evaluation Module
  Enhanced evaluation with advanced features for better position assessment
*/

#ifndef EVALUATE_ENHANCED_H_INCLUDED
#define EVALUATE_ENHANCED_H_INCLUDED

#include "evaluate.h"
#include "position.h"
#include "types.h"
#include <array>
#include <unordered_map>

namespace Stockfish {

namespace EvalEnhanced {

// Enhanced evaluation weights and parameters
struct EvalParams {
    // Material values with piece-square adjustments
    static constexpr Value PAWN_VALUE = Value(100);
    static constexpr Value KNIGHT_VALUE = Value(320);
    static constexpr Value BISHOP_VALUE = Value(330);
    static constexpr Value ROOK_VALUE = Value(500);
    static constexpr Value QUEEN_VALUE = Value(900);
    
    // King safety parameters
    static constexpr Value KING_SAFETY_BASE = Value(50);
    static constexpr Value PAWN_SHELTER_BONUS = Value(15);
    static constexpr Value PAWN_STORM_PENALTY = Value(10);
    
    // Piece mobility bonuses
    static constexpr Value KNIGHT_MOBILITY[9] = {
        Value(-25), Value(-15), Value(-5), Value(0), Value(5), 
        Value(10), Value(15), Value(20), Value(25)
    };
    
    static constexpr Value BISHOP_MOBILITY[14] = {
        Value(-30), Value(-20), Value(-10), Value(-5), Value(0), Value(5), 
        Value(10), Value(15), Value(20), Value(25), Value(30), Value(35), 
        Value(40), Value(45)
    };
    
    static constexpr Value ROOK_MOBILITY[15] = {
        Value(-40), Value(-25), Value(-15), Value(-10), Value(-5), Value(0), 
        Value(5), Value(10), Value(15), Value(20), Value(25), Value(30), 
        Value(35), Value(40), Value(45)
    };
    
    static constexpr Value QUEEN_MOBILITY[28] = {
        Value(-50), Value(-35), Value(-25), Value(-15), Value(-10), Value(-5), 
        Value(0), Value(3), Value(6), Value(9), Value(12), Value(15), 
        Value(18), Value(21), Value(24), Value(27), Value(30), Value(33), 
        Value(36), Value(39), Value(42), Value(45), Value(48), Value(51), 
        Value(54), Value(57), Value(60), Value(65)
    };
};

// Enhanced pawn structure analysis
class PawnStructure {
public:
    static Value evaluate_pawn_structure(const Position& pos, Color color);
    static Value evaluate_pawn_chains(const Position& pos, Color color);
    static Value evaluate_pawn_islands(const Position& pos, Color color);
    static Value evaluate_passed_pawns(const Position& pos, Color color);
    static Value evaluate_doubled_pawns(const Position& pos, Color color);
    static Value evaluate_isolated_pawns(const Position& pos, Color color);
    static Value evaluate_backward_pawns(const Position& pos, Color color);

private:
    static bool is_passed_pawn(const Position& pos, Square sq, Color color);
    static bool is_isolated_pawn(const Position& pos, Square sq, Color color);
    static bool is_backward_pawn(const Position& pos, Square sq, Color color);
    static int count_pawn_islands(const Position& pos, Color color);
};

// Enhanced king safety evaluation
class KingSafety {
public:
    static Value evaluate_king_safety(const Position& pos, Color color);
    static Value evaluate_pawn_shelter(const Position& pos, Color color);
    static Value evaluate_pawn_storm(const Position& pos, Color color);
    static Value evaluate_king_attackers(const Position& pos, Color color);
    static Value evaluate_king_zone_control(const Position& pos, Color color);

private:
    static Bitboard get_king_zone(Square king_sq, Color color);
    static int count_king_attackers(const Position& pos, Square king_sq, Color attacking_color);
    static Value calculate_attack_weight(PieceType piece, int attack_count);
};

// Enhanced piece evaluation
class PieceEvaluation {
public:
    static Value evaluate_knights(const Position& pos, Color color);
    static Value evaluate_bishops(const Position& pos, Color color);
    static Value evaluate_rooks(const Position& pos, Color color);
    static Value evaluate_queens(const Position& pos, Color color);
    static Value evaluate_piece_coordination(const Position& pos, Color color);

private:
    static Value evaluate_knight_outposts(const Position& pos, Color color);
    static Value evaluate_bishop_pair(const Position& pos, Color color);
    static Value evaluate_rook_open_files(const Position& pos, Color color);
    static Value evaluate_piece_mobility(const Position& pos, PieceType piece, Square sq, Color color);
    static bool is_outpost(const Position& pos, Square sq, Color color);
};

// Space evaluation
class SpaceEvaluation {
public:
    static Value evaluate_space(const Position& pos, Color color);
    static Value evaluate_central_control(const Position& pos, Color color);
    static Value evaluate_advanced_pawns(const Position& pos, Color color);

private:
    static Bitboard get_space_mask(Color color);
    static Bitboard get_center_squares();
    static Value calculate_space_bonus(int space_count, int piece_count);
};

// Threats and tactical features
class ThreatEvaluation {
public:
    static Value evaluate_threats(const Position& pos, Color color);
    static Value evaluate_hanging_pieces(const Position& pos, Color color);
    static Value evaluate_pins_and_forks(const Position& pos, Color color);
    static Value evaluate_discovered_attacks(const Position& pos, Color color);

private:
    static bool is_hanging(const Position& pos, Square sq, Color color);
    static Value calculate_threat_bonus(PieceType attacker, PieceType victim);
};

// Endgame evaluation enhancements
class EndgameEvaluation {
public:
    static Value evaluate_endgame_factors(const Position& pos);
    static Value evaluate_king_activity(const Position& pos, Color color);
    static Value evaluate_opposition(const Position& pos);
    static Value evaluate_pawn_endgame(const Position& pos);
    static Value evaluate_piece_endgame(const Position& pos);

private:
    static bool is_endgame(const Position& pos);
    static bool has_opposition(const Position& pos, Color color);
    static Value calculate_king_distance_to_pawns(const Position& pos, Color color);
};

// Position pattern recognition
class PatternRecognition {
public:
    static Value recognize_common_patterns(const Position& pos);
    static Value evaluate_piece_patterns(const Position& pos, Color color);
    static Value evaluate_pawn_patterns(const Position& pos, Color color);

private:
    static bool is_fianchetto(const Position& pos, Color color);
    static bool is_dragon_formation(const Position& pos, Color color);
    static bool is_stonewall_formation(const Position& pos, Color color);
    static Value calculate_pattern_bonus(const std::string& pattern_name);
};

// Main enhanced evaluation class
class EnhancedEvaluator {
public:
    EnhancedEvaluator();
    
    Value evaluate(const Position& pos);
    Value evaluate_from_perspective(const Position& pos, Color color);
    
    // Component evaluations
    Value evaluate_material(const Position& pos, Color color);
    Value evaluate_positional(const Position& pos, Color color);
    Value evaluate_tactical(const Position& pos, Color color);
    Value evaluate_endgame_bonus(const Position& pos, Color color);
    
    // Evaluation caching
    struct EvalCacheEntry {
        uint64_t key;
        Value mg_value;
        Value eg_value;
        uint32_t age;
    };
    
    void clear_cache();
    
private:
    // Evaluation cache for complex positions
    std::unordered_map<uint64_t, EvalCacheEntry> eval_cache;
    uint32_t cache_age;
    
    // Phase calculation for interpolation between middlegame and endgame
    int calculate_game_phase(const Position& pos);
    Value interpolate_eval(Value mg_value, Value eg_value, int phase);
    
    // Component weights
    struct EvalWeights {
        Value material_weight = Value(100);
        Value positional_weight = Value(80);
        Value king_safety_weight = Value(60);
        Value pawn_structure_weight = Value(40);
        Value piece_coordination_weight = Value(30);
        Value space_weight = Value(20);
        Value threat_weight = Value(25);
        Value pattern_weight = Value(15);
    } weights;
    
    // Helper methods
    Value combine_evaluations(const Position& pos, Color color);
    bool should_use_cache(const Position& pos);
    void store_in_cache(const Position& pos, Value mg_value, Value eg_value);
};

// Evaluation tuning and optimization
class EvalTuner {
public:
    static void tune_parameters();
    static void test_evaluation_accuracy();
    static void benchmark_evaluation_speed();
    
private:
    static void genetic_algorithm_tuning();
    static void gradient_descent_tuning();
    static double calculate_evaluation_error(const std::vector<Position>& positions);
};

} // namespace EvalEnhanced

} // namespace Stockfish

#endif // #ifndef EVALUATE_ENHANCED_H_INCLUDED