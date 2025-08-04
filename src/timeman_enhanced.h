/*
  Stockfish Enhanced Time Management
  Adaptive time allocation based on position complexity and game phase
*/

#ifndef TIMEMAN_ENHANCED_H_INCLUDED
#define TIMEMAN_ENHANCED_H_INCLUDED

#include "timeman.h"
#include "position.h"
#include "types.h"
#include <chrono>
#include <vector>

namespace Stockfish {

namespace TimeManagement {

// Enhanced time management parameters
struct TimeParams {
    static constexpr double COMPLEXITY_BASE_FACTOR = 1.0;
    static constexpr double COMPLEXITY_MAX_FACTOR = 2.5;
    static constexpr double ENDGAME_TIME_FACTOR = 1.3;
    static constexpr double CRITICAL_POSITION_FACTOR = 2.0;
    static constexpr double BOOK_MOVE_TIME_SAVE = 0.1;
    static constexpr double PANIC_TIME_THRESHOLD = 0.05; // 5% of remaining time
    static constexpr int MIN_THINKING_TIME_MS = 100;
    static constexpr int MAX_THINKING_TIME_MS = 30000;
};

// Position complexity analyzer
class ComplexityAnalyzer {
public:
    static double calculate_position_complexity(const Position& pos);
    static double calculate_tactical_complexity(const Position& pos);
    static double calculate_strategic_complexity(const Position& pos);
    static double calculate_endgame_complexity(const Position& pos);
    
private:
    static double count_piece_mobility(const Position& pos);
    static double count_captures_and_checks(const Position& pos);
    static double analyze_pawn_structure_complexity(const Position& pos);
    static double analyze_king_safety_complexity(const Position& pos);
    static bool is_complex_endgame(const Position& pos);
};

// Game phase detector
class GamePhaseDetector {
public:
    enum GamePhase {
        OPENING,
        MIDDLEGAME,
        ENDGAME,
        LATE_ENDGAME
    };
    
    static GamePhase detect_game_phase(const Position& pos);
    static double get_phase_progress(const Position& pos);
    static bool is_critical_phase_transition(const Position& pos, GamePhase prev_phase);
    
private:
    static int calculate_material_count(const Position& pos);
    static bool has_major_pieces(const Position& pos);
    static int count_pawns(const Position& pos);
};

// Position criticality assessment
class CriticalityAssessment {
public:
    static double assess_position_criticality(const Position& pos, Value eval, Value prev_eval);
    static bool is_critical_position(const Position& pos);
    static double calculate_evaluation_volatility(const std::vector<Value>& recent_evals);
    
private:
    static bool has_tactical_threats(const Position& pos);
    static bool is_near_promotion(const Position& pos);
    static bool is_king_under_attack(const Position& pos);
    static bool has_hanging_pieces(const Position& pos);
};

// Search statistics for time allocation
struct SearchStatistics {
    uint64_t nodes_searched;
    uint64_t beta_cutoffs;
    uint64_t tt_hits;
    double branching_factor;
    int depth_achieved;
    TimePoint search_start_time;
    std::vector<Value> iteration_scores;
    std::vector<TimePoint> iteration_times;
    
    void reset();
    void update_iteration(Value score, TimePoint time);
    double get_score_stability() const;
    double get_time_efficiency() const;
};

// Enhanced time manager
class EnhancedTimeManager {
public:
    EnhancedTimeManager();
    
    // Main time allocation methods
    TimePoint calculate_optimal_time(const Position& pos, const Search::LimitsType& limits);
    TimePoint calculate_move_time(const Position& pos, TimePoint remaining_time, 
                                 int moves_to_go, TimePoint increment);
    
    // Dynamic time adjustment during search
    void update_search_progress(const SearchStatistics& stats);
    bool should_stop_search(const SearchStatistics& stats, TimePoint allocated_time);
    TimePoint adjust_time_during_search(const SearchStatistics& stats, TimePoint original_time);
    
    // Time allocation factors
    double calculate_complexity_factor(const Position& pos);
    double calculate_phase_factor(const Position& pos);
    double calculate_criticality_factor(const Position& pos);
    double calculate_stability_factor(const SearchStatistics& stats);
    
    // Move importance assessment
    double assess_move_importance(const Position& pos, Move move);
    bool is_forced_move(const Position& pos);
    bool is_book_move(const Position& pos);
    
    void set_position_history(const std::vector<Value>& eval_history);
    void reset_for_new_game();
    
private:
    SearchStatistics current_stats;
    std::vector<Value> evaluation_history;
    GamePhaseDetector::GamePhase last_game_phase;
    TimePoint base_time_allocation;
    double time_pressure_factor;
    
    // Adaptive parameters that learn during the game
    struct AdaptiveParams {
        double complexity_sensitivity = 1.0;
        double time_efficiency_factor = 1.0;
        double safety_margin = 0.1;
        double panic_threshold = 0.05;
    } adaptive_params;
    
    // Helper methods
    TimePoint apply_time_factors(TimePoint base_time, double complexity_factor,
                                double phase_factor, double criticality_factor);
    TimePoint apply_safety_margins(TimePoint calculated_time, TimePoint remaining_time);
    void update_adaptive_parameters(const SearchStatistics& stats);
    bool is_in_time_trouble(TimePoint remaining_time, int moves_to_go);
};

// Time allocation strategies for different game phases
class TimeAllocationStrategy {
public:
    static TimePoint allocate_opening_time(TimePoint total_time, int moves_played);
    static TimePoint allocate_middlegame_time(TimePoint total_time, double complexity);
    static TimePoint allocate_endgame_time(TimePoint total_time, int pieces_remaining);
    
private:
    static constexpr double OPENING_TIME_FRACTION = 0.15;
    static constexpr double MIDDLEGAME_TIME_FRACTION = 0.60;
    static constexpr double ENDGAME_TIME_FRACTION = 0.25;
};

// Emergency time management for time trouble
class EmergencyTimeManager {
public:
    static TimePoint calculate_emergency_time(TimePoint remaining_time, int moves_to_go);
    static bool should_use_emergency_mode(TimePoint remaining_time, TimePoint increment);
    static TimePoint get_minimum_move_time(const Position& pos);
    
private:
    static constexpr TimePoint EMERGENCY_TIME_THRESHOLD = TimePoint(30000); // 30 seconds
    static constexpr TimePoint MINIMUM_MOVE_TIME = TimePoint(100); // 100 ms
    static constexpr double EMERGENCY_TIME_FACTOR = 0.8;
};

// Time management for different time controls
class TimeControlAdapter {
public:
    enum TimeControlType {
        CLASSICAL,      // Long time controls (90+ minutes)
        RAPID,          // Medium time controls (15-60 minutes)
        BLITZ,          // Fast time controls (3-15 minutes)
        BULLET,         // Very fast time controls (<3 minutes)
        INCREMENT,      // Games with increment
        FIXED_TIME      // Fixed time per move
    };
    
    static TimeControlType detect_time_control(const Search::LimitsType& limits);
    static double get_time_control_factor(TimeControlType type);
    static TimePoint adapt_for_time_control(TimePoint base_time, TimeControlType type);
    
private:
    static constexpr TimePoint BLITZ_THRESHOLD = TimePoint(900000);  // 15 minutes
    static constexpr TimePoint BULLET_THRESHOLD = TimePoint(180000); // 3 minutes
};

// Performance monitoring and optimization
class TimeManagementProfiler {
public:
    struct TimeUsageStats {
        TimePoint total_time_used;
        TimePoint average_move_time;
        TimePoint longest_move_time;
        TimePoint shortest_move_time;
        double time_accuracy_score;
        int moves_in_time_trouble;
        int emergency_moves;
    };
    
    void record_move_time(TimePoint allocated_time, TimePoint actual_time, 
                         bool was_good_move);
    TimeUsageStats get_statistics() const;
    void suggest_improvements();
    void reset_statistics();
    
private:
    std::vector<TimePoint> allocated_times;
    std::vector<TimePoint> actual_times;
    std::vector<bool> move_quality;
    TimeUsageStats stats;
    
    void calculate_statistics();
};

} // namespace TimeManagement

} // namespace Stockfish

#endif // #ifndef TIMEMAN_ENHANCED_H_INCLUDED