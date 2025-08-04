/*
  Stockfish Enhanced Evaluation Implementation
  Advanced position evaluation with sophisticated features
*/

#include "evaluate_enhanced.h"
#include "bitboard.h"
#include "material.h"
#include "position.h"
#include "thread.h"
#include <algorithm>
#include <cmath>

namespace Stockfish {

namespace EvalEnhanced {

// Pawn Structure Implementation
Value PawnStructure::evaluate_pawn_structure(const Position& pos, Color color) {
    Value total = VALUE_ZERO;
    
    total += evaluate_pawn_chains(pos, color);
    total += evaluate_pawn_islands(pos, color);
    total += evaluate_passed_pawns(pos, color);
    total += evaluate_doubled_pawns(pos, color);
    total += evaluate_isolated_pawns(pos, color);
    total += evaluate_backward_pawns(pos, color);
    
    return total;
}

Value PawnStructure::evaluate_pawn_chains(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    Bitboard pawns = pos.pieces(color, PAWN);
    
    while (pawns) {
        Square sq = pop_lsb(pawns);
        Bitboard supports = pos.pieces(color, PAWN) & pawn_attacks_bb(~color, sq);
        
        if (supports) {
            // Bonus for pawn chains, more for advanced pawns
            int chain_length = popcount(supports) + 1;
            int advancement = color == WHITE ? rank_of(sq) - RANK_2 : RANK_7 - rank_of(sq);
            bonus += Value(chain_length * 8 + advancement * 4);
        }
    }
    
    return bonus;
}

Value PawnStructure::evaluate_pawn_islands(const Position& pos, Color color) {
    int islands = count_pawn_islands(pos, color);
    // Penalty for each additional island beyond 1
    return Value(-(islands - 1) * 15);
}

Value PawnStructure::evaluate_passed_pawns(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    Bitboard pawns = pos.pieces(color, PAWN);
    
    while (pawns) {
        Square sq = pop_lsb(pawns);
        if (is_passed_pawn(pos, sq, color)) {
            int advancement = color == WHITE ? rank_of(sq) - RANK_2 : RANK_7 - rank_of(sq);
            // Exponential bonus for advanced passed pawns
            bonus += Value(20 + advancement * advancement * 5);
            
            // Additional bonus if protected by own pawns
            if (pos.pieces(color, PAWN) & pawn_attacks_bb(~color, sq)) {
                bonus += Value(10 + advancement * 3);
            }
        }
    }
    
    return bonus;
}

Value PawnStructure::evaluate_doubled_pawns(const Position& pos, Color color) {
    Value penalty = VALUE_ZERO;
    Bitboard pawns = pos.pieces(color, PAWN);
    
    for (File f = FILE_A; f <= FILE_H; ++f) {
        int pawn_count = popcount(pawns & file_bb(f));
        if (pawn_count > 1) {
            penalty += Value((pawn_count - 1) * 12);
        }
    }
    
    return -penalty;
}

Value PawnStructure::evaluate_isolated_pawns(const Position& pos, Color color) {
    Value penalty = VALUE_ZERO;
    Bitboard pawns = pos.pieces(color, PAWN);
    
    while (pawns) {
        Square sq = pop_lsb(pawns);
        if (is_isolated_pawn(pos, sq, color)) {
            penalty += Value(20);
            
            // Additional penalty in endgame
            if (pos.non_pawn_material() < 2 * EvalParams::ROOK_VALUE) {
                penalty += Value(10);
            }
        }
    }
    
    return -penalty;
}

Value PawnStructure::evaluate_backward_pawns(const Position& pos, Color color) {
    Value penalty = VALUE_ZERO;
    Bitboard pawns = pos.pieces(color, PAWN);
    
    while (pawns) {
        Square sq = pop_lsb(pawns);
        if (is_backward_pawn(pos, sq, color)) {
            penalty += Value(15);
        }
    }
    
    return -penalty;
}

bool PawnStructure::is_passed_pawn(const Position& pos, Square sq, Color color) {
    const Direction Up = color == WHITE ? NORTH : SOUTH;
    const Bitboard enemy_pawns = pos.pieces(~color, PAWN);
    
    Bitboard front_span = 0;
    for (Square s = sq + Up; is_ok(s); s += Up) {
        front_span |= square_bb(s);
        if (file_of(s) > FILE_A) front_span |= square_bb(s - 1);
        if (file_of(s) < FILE_H) front_span |= square_bb(s + 1);
    }
    
    return !(enemy_pawns & front_span);
}

bool PawnStructure::is_isolated_pawn(const Position& pos, Square sq, Color color) {
    const File f = file_of(sq);
    Bitboard adjacent_files = 0;
    
    if (f > FILE_A) adjacent_files |= file_bb(File(f - 1));
    if (f < FILE_H) adjacent_files |= file_bb(File(f + 1));
    
    return !(pos.pieces(color, PAWN) & adjacent_files);
}

bool PawnStructure::is_backward_pawn(const Position& pos, Square sq, Color color) {
    const Direction Up = color == WHITE ? NORTH : SOUTH;
    const File f = file_of(sq);
    
    // Check if pawn can advance safely
    Square advance_sq = sq + Up;
    if (!is_ok(advance_sq) || pos.piece_on(advance_sq) != NO_PIECE) {
        return false;
    }
    
    // Check if advancing would be attacked by enemy pawns
    if (pos.pieces(~color, PAWN) & pawn_attacks_bb(color, advance_sq)) {
        // Check if can be supported by own pawns
        Bitboard support_files = 0;
        if (f > FILE_A) support_files |= file_bb(File(f - 1));
        if (f < FILE_H) support_files |= file_bb(File(f + 1));
        
        Bitboard supporting_pawns = pos.pieces(color, PAWN) & support_files;
        bool can_be_supported = false;
        
        while (supporting_pawns) {
            Square support_sq = pop_lsb(supporting_pawns);
            int support_rank = color == WHITE ? rank_of(support_sq) : 7 - rank_of(support_sq);
            int pawn_rank = color == WHITE ? rank_of(sq) : 7 - rank_of(sq);
            
            if (support_rank <= pawn_rank) {
                can_be_supported = true;
                break;
            }
        }
        
        return !can_be_supported;
    }
    
    return false;
}

int PawnStructure::count_pawn_islands(const Position& pos, Color color) {
    Bitboard pawns = pos.pieces(color, PAWN);
    int islands = 0;
    bool in_island = false;
    
    for (File f = FILE_A; f <= FILE_H; ++f) {
        bool has_pawn = pawns & file_bb(f);
        
        if (has_pawn && !in_island) {
            islands++;
            in_island = true;
        } else if (!has_pawn) {
            in_island = false;
        }
    }
    
    return islands;
}

// King Safety Implementation
Value KingSafety::evaluate_king_safety(const Position& pos, Color color) {
    Value total = VALUE_ZERO;
    
    total += evaluate_pawn_shelter(pos, color);
    total += evaluate_pawn_storm(pos, color);
    total += evaluate_king_attackers(pos, color);
    total += evaluate_king_zone_control(pos, color);
    
    return total;
}

Value KingSafety::evaluate_pawn_shelter(const Position& pos, Color color) {
    Square king_sq = pos.square<KING>(color);
    Value bonus = VALUE_ZERO;
    
    const Direction Up = color == WHITE ? NORTH : SOUTH;
    const File king_file = file_of(king_sq);
    
    // Check pawn shelter in front of king
    for (int file_offset = -1; file_offset <= 1; ++file_offset) {
        File f = File(king_file + file_offset);
        if (f < FILE_A || f > FILE_H) continue;
        
        Bitboard file_pawns = pos.pieces(color, PAWN) & file_bb(f);
        if (file_pawns) {
            Square closest_pawn = color == WHITE ? 
                lsb(file_pawns) : msb(file_pawns);
            int distance = std::abs(rank_of(closest_pawn) - rank_of(king_sq));
            
            if (distance <= 2) {
                bonus += EvalParams::PAWN_SHELTER_BONUS / (distance + 1);
            }
        } else {
            // Penalty for missing shelter
            bonus -= Value(15);
        }
    }
    
    return bonus;
}

Value KingSafety::evaluate_pawn_storm(const Position& pos, Color color) {
    Square king_sq = pos.square<KING>(color);
    Value penalty = VALUE_ZERO;
    
    const Direction Up = color == WHITE ? NORTH : SOUTH;
    const File king_file = file_of(king_sq);
    
    // Check enemy pawn storm
    for (int file_offset = -2; file_offset <= 2; ++file_offset) {
        File f = File(king_file + file_offset);
        if (f < FILE_A || f > FILE_H) continue;
        
        Bitboard enemy_pawns = pos.pieces(~color, PAWN) & file_bb(f);
        if (enemy_pawns) {
            Square closest_pawn = color == WHITE ? 
                msb(enemy_pawns) : lsb(enemy_pawns);
            int distance = std::abs(rank_of(closest_pawn) - rank_of(king_sq));
            
            if (distance <= 3) {
                penalty += EvalParams::PAWN_STORM_PENALTY * (4 - distance);
            }
        }
    }
    
    return -penalty;
}

Value KingSafety::evaluate_king_attackers(const Position& pos, Color color) {
    Square king_sq = pos.square<KING>(color);
    int attacker_count = count_king_attackers(pos, king_sq, ~color);
    
    if (attacker_count == 0) return VALUE_ZERO;
    
    // Exponential penalty based on number of attackers
    Value penalty = Value(attacker_count * attacker_count * 15);
    
    // Increase penalty if queen is attacking
    if (pos.attackers_to(king_sq) & pos.pieces(~color, QUEEN)) {
        penalty += Value(50);
    }
    
    return -penalty;
}

Value KingSafety::evaluate_king_zone_control(const Position& pos, Color color) {
    Square king_sq = pos.square<KING>(color);
    Bitboard king_zone = get_king_zone(king_sq, color);
    
    int our_control = popcount(king_zone & pos.attackers_to_all(color));
    int enemy_control = popcount(king_zone & pos.attackers_to_all(~color));
    
    return Value((our_control - enemy_control) * 8);
}

Bitboard KingSafety::get_king_zone(Square king_sq, Color color) {
    return attacks_bb<KING>(king_sq) | square_bb(king_sq);
}

int KingSafety::count_king_attackers(const Position& pos, Square king_sq, Color attacking_color) {
    Bitboard attackers = pos.attackers_to(king_sq) & pos.pieces(attacking_color);
    return popcount(attackers);
}

Value KingSafety::calculate_attack_weight(PieceType piece, int attack_count) {
    static const Value weights[PIECE_TYPE_NB] = {
        VALUE_ZERO, Value(5), Value(15), Value(15), Value(20), Value(30), VALUE_ZERO
    };
    
    return weights[piece] * attack_count;
}

// Piece Evaluation Implementation
Value PieceEvaluation::evaluate_knights(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    Bitboard knights = pos.pieces(color, KNIGHT);
    
    while (knights) {
        Square sq = pop_lsb(knights);
        
        // Mobility bonus
        int mobility = popcount(attacks_bb<KNIGHT>(sq) & ~pos.pieces(color));
        bonus += EvalParams::KNIGHT_MOBILITY[std::min(mobility, 8)];
        
        // Outpost bonus
        if (is_outpost(pos, sq, color)) {
            bonus += Value(25);
        }
        
        // Center control bonus
        if (sq == SQ_D4 || sq == SQ_D5 || sq == SQ_E4 || sq == SQ_E5) {
            bonus += Value(15);
        }
    }
    
    return bonus;
}

Value PieceEvaluation::evaluate_bishops(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    Bitboard bishops = pos.pieces(color, BISHOP);
    
    // Bishop pair bonus
    if (popcount(bishops) >= 2) {
        bonus += Value(50);
    }
    
    while (bishops) {
        Square sq = pop_lsb(bishops);
        
        // Mobility bonus
        int mobility = popcount(attacks_bb<BISHOP>(sq, pos.pieces()) & ~pos.pieces(color));
        bonus += EvalParams::BISHOP_MOBILITY[std::min(mobility, 13)];
        
        // Long diagonal bonus
        if (sq == SQ_A1 || sq == SQ_H8 || sq == SQ_A8 || sq == SQ_H1) {
            bonus += Value(10);
        }
    }
    
    return bonus;
}

Value PieceEvaluation::evaluate_rooks(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    Bitboard rooks = pos.pieces(color, ROOK);
    
    while (rooks) {
        Square sq = pop_lsb(rooks);
        
        // Mobility bonus
        int mobility = popcount(attacks_bb<ROOK>(sq, pos.pieces()) & ~pos.pieces(color));
        bonus += EvalParams::ROOK_MOBILITY[std::min(mobility, 14)];
        
        // Open file bonus
        File f = file_of(sq);
        if (!(pos.pieces(PAWN) & file_bb(f))) {
            bonus += Value(25); // Open file
        } else if (!(pos.pieces(color, PAWN) & file_bb(f))) {
            bonus += Value(15); // Semi-open file
        }
        
        // 7th rank bonus
        Rank r = rank_of(sq);
        if ((color == WHITE && r == RANK_7) || (color == BLACK && r == RANK_2)) {
            bonus += Value(20);
        }
    }
    
    return bonus;
}

Value PieceEvaluation::evaluate_queens(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    Bitboard queens = pos.pieces(color, QUEEN);
    
    while (queens) {
        Square sq = pop_lsb(queens);
        
        // Mobility bonus (capped to prevent too early development)
        int mobility = popcount(attacks_bb<QUEEN>(sq, pos.pieces()) & ~pos.pieces(color));
        bonus += EvalParams::QUEEN_MOBILITY[std::min(mobility, 27)];
        
        // Penalty for early development
        if (pos.count<KNIGHT>(color) + pos.count<BISHOP>(color) > 2) {
            int queen_rank = color == WHITE ? rank_of(sq) : 7 - rank_of(sq);
            if (queen_rank > 3) {
                bonus -= Value(20);
            }
        }
    }
    
    return bonus;
}

Value PieceEvaluation::evaluate_piece_coordination(const Position& pos, Color color) {
    Value bonus = VALUE_ZERO;
    
    // Rook + Queen coordination on same file/rank
    Bitboard rooks = pos.pieces(color, ROOK);
    Bitboard queens = pos.pieces(color, QUEEN);
    
    while (rooks) {
        Square rook_sq = pop_lsb(rooks);
        
        // Check for queen on same file or rank
        if ((queens & file_bb(file_of(rook_sq))) || 
            (queens & rank_bb(rank_of(rook_sq)))) {
            bonus += Value(15);
        }
    }
    
    // Bishop + Knight coordination
    Bitboard bishops = pos.pieces(color, BISHOP);
    Bitboard knights = pos.pieces(color, KNIGHT);
    
    if (bishops && knights) {
        bonus += Value(10); // Basic coordination bonus
    }
    
    return bonus;
}

bool PieceEvaluation::is_outpost(const Position& pos, Square sq, Color color) {
    // Check if square is supported by pawns and can't be attacked by enemy pawns
    if (!(pos.pieces(color, PAWN) & pawn_attacks_bb(~color, sq))) {
        return false;
    }
    
    // Check if enemy pawns can attack this square
    const Direction Up = color == WHITE ? NORTH : SOUTH;
    const File f = file_of(sq);
    
    for (int file_offset = -1; file_offset <= 1; file_offset += 2) {
        File check_file = File(f + file_offset);
        if (check_file < FILE_A || check_file > FILE_H) continue;
        
        for (Square s = sq - Up; is_ok(s); s -= Up) {
            if (pos.piece_on(s) == make_piece(~color, PAWN)) {
                return false;
            }
        }
    }
    
    return true;
}

// Space Evaluation Implementation
Value SpaceEvaluation::evaluate_space(const Position& pos, Color color) {
    Bitboard space_mask = get_space_mask(color);
    Bitboard our_pieces = pos.pieces(color);
    Bitboard controlled_space = 0;
    
    // Calculate space controlled by our pieces
    for (PieceType pt = PAWN; pt <= KING; ++pt) {
        Bitboard pieces = pos.pieces(color, pt);
        while (pieces) {
            Square sq = pop_lsb(pieces);
            controlled_space |= attacks_bb(pt, sq, pos.pieces()) & space_mask;
        }
    }
    
    int space_count = popcount(controlled_space);
    int piece_count = popcount(our_pieces) - popcount(pos.pieces(color, KING));
    
    return calculate_space_bonus(space_count, piece_count);
}

Value SpaceEvaluation::evaluate_central_control(const Position& pos, Color color) {
    Bitboard center = get_center_squares();
    Bitboard our_attacks = 0;
    
    for (PieceType pt = PAWN; pt <= KING; ++pt) {
        Bitboard pieces = pos.pieces(color, pt);
        while (pieces) {
            Square sq = pop_lsb(pieces);
            our_attacks |= attacks_bb(pt, sq, pos.pieces());
        }
    }
    
    int center_control = popcount(our_attacks & center);
    return Value(center_control * 8);
}

Bitboard SpaceEvaluation::get_space_mask(Color color) {
    return color == WHITE ? 
        (Rank4BB | Rank5BB | Rank6BB | Rank7BB) :
        (Rank5BB | Rank4BB | Rank3BB | Rank2BB);
}

Bitboard SpaceEvaluation::get_center_squares() {
    return square_bb(SQ_D4) | square_bb(SQ_D5) | 
           square_bb(SQ_E4) | square_bb(SQ_E5);
}

Value SpaceEvaluation::calculate_space_bonus(int space_count, int piece_count) {
    if (piece_count <= 2) return VALUE_ZERO;
    
    return Value(space_count * piece_count / 16);
}

// Enhanced Evaluator Implementation
EnhancedEvaluator::EnhancedEvaluator() : cache_age(0) {}

Value EnhancedEvaluator::evaluate(const Position& pos) {
    // Check cache first
    uint64_t key = pos.key();
    if (should_use_cache(pos)) {
        auto it = eval_cache.find(key);
        if (it != eval_cache.end() && it->second.age == cache_age) {
            int phase = calculate_game_phase(pos);
            return interpolate_eval(it->second.mg_value, it->second.eg_value, phase);
        }
    }
    
    // Calculate evaluation for both sides
    Value mg_white = evaluate_from_perspective(pos, WHITE);
    Value eg_white = evaluate_from_perspective(pos, WHITE);
    Value mg_black = evaluate_from_perspective(pos, BLACK);
    Value eg_black = evaluate_from_perspective(pos, BLACK);
    
    Value mg_eval = mg_white - mg_black;
    Value eg_eval = eg_white - eg_black;
    
    // Store in cache
    if (should_use_cache(pos)) {
        store_in_cache(pos, mg_eval, eg_eval);
    }
    
    // Interpolate based on game phase
    int phase = calculate_game_phase(pos);
    Value final_eval = interpolate_eval(mg_eval, eg_eval, phase);
    
    // Return from perspective of side to move
    return pos.side_to_move() == WHITE ? final_eval : -final_eval;
}

Value EnhancedEvaluator::evaluate_from_perspective(const Position& pos, Color color) {
    Value total = VALUE_ZERO;
    
    total += evaluate_material(pos, color) * weights.material_weight / 100;
    total += evaluate_positional(pos, color) * weights.positional_weight / 100;
    total += KingSafety::evaluate_king_safety(pos, color) * weights.king_safety_weight / 100;
    total += PawnStructure::evaluate_pawn_structure(pos, color) * weights.pawn_structure_weight / 100;
    total += PieceEvaluation::evaluate_piece_coordination(pos, color) * weights.piece_coordination_weight / 100;
    total += SpaceEvaluation::evaluate_space(pos, color) * weights.space_weight / 100;
    
    return total;
}

Value EnhancedEvaluator::evaluate_material(const Position& pos, Color color) {
    Value material = VALUE_ZERO;
    
    material += pos.count<PAWN>(color) * EvalParams::PAWN_VALUE;
    material += pos.count<KNIGHT>(color) * EvalParams::KNIGHT_VALUE;
    material += pos.count<BISHOP>(color) * EvalParams::BISHOP_VALUE;
    material += pos.count<ROOK>(color) * EvalParams::ROOK_VALUE;
    material += pos.count<QUEEN>(color) * EvalParams::QUEEN_VALUE;
    
    return material;
}

Value EnhancedEvaluator::evaluate_positional(const Position& pos, Color color) {
    Value positional = VALUE_ZERO;
    
    positional += PieceEvaluation::evaluate_knights(pos, color);
    positional += PieceEvaluation::evaluate_bishops(pos, color);
    positional += PieceEvaluation::evaluate_rooks(pos, color);
    positional += PieceEvaluation::evaluate_queens(pos, color);
    
    return positional;
}

int EnhancedEvaluator::calculate_game_phase(const Position& pos) {
    // Calculate game phase based on remaining material
    int phase = 0;
    phase += popcount(pos.pieces(KNIGHT)) * 1;
    phase += popcount(pos.pieces(BISHOP)) * 1;
    phase += popcount(pos.pieces(ROOK)) * 2;
    phase += popcount(pos.pieces(QUEEN)) * 4;
    
    // Normalize to 0-256 range
    phase = std::min(phase, 24);
    return (phase * 256 + 12) / 24;
}

Value EnhancedEvaluator::interpolate_eval(Value mg_value, Value eg_value, int phase) {
    return ((mg_value * (256 - phase)) + (eg_value * phase)) / 256;
}

bool EnhancedEvaluator::should_use_cache(const Position& pos) {
    // Use cache for positions with sufficient material complexity
    return pos.non_pawn_material() > EvalParams::ROOK_VALUE;
}

void EnhancedEvaluator::store_in_cache(const Position& pos, Value mg_value, Value eg_value) {
    uint64_t key = pos.key();
    eval_cache[key] = {key, mg_value, eg_value, cache_age};
    
    // Limit cache size
    if (eval_cache.size() > 100000) {
        eval_cache.clear();
        cache_age++;
    }
}

void EnhancedEvaluator::clear_cache() {
    eval_cache.clear();
    cache_age++;
}

} // namespace EvalEnhanced

} // namespace Stockfish