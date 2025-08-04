#!/usr/bin/env python3
"""
Comprehensive test suite for Stockfish enhancements
Tests the enhanced search, evaluation, and time management features
"""

import subprocess
import sys
import time
import chess
import chess.engine
import json
from typing import List, Dict, Tuple
from dataclasses import dataclass

@dataclass
class TestResult:
    test_name: str
    passed: bool
    execution_time: float
    details: str = ""
    score: float = 0.0

class StockfishEnhancementTester:
    def __init__(self, stockfish_path: str = "../src/stockfish"):
        self.stockfish_path = stockfish_path
        self.test_results: List[TestResult] = []
        
    def run_all_tests(self) -> Dict[str, any]:
        """Run all enhancement tests and return comprehensive results"""
        print("Starting Stockfish Enhancement Test Suite...")
        print("=" * 50)
        
        # Test compilation
        compilation_result = self.test_compilation()
        if not compilation_result.passed:
            print(f"CRITICAL: Compilation failed - {compilation_result.details}")
            return {"compilation_failed": True, "results": [compilation_result]}
        
        # Core functionality tests
        self.test_basic_functionality()
        self.test_enhanced_search()
        self.test_enhanced_evaluation()
        self.test_time_management()
        self.test_performance_benchmarks()
        self.test_tactical_positions()
        self.test_endgame_positions()
        
        return self.generate_report()
    
    def test_compilation(self) -> TestResult:
        """Test if enhanced Stockfish compiles successfully"""
        print("Testing compilation...")
        start_time = time.time()
        
        try:
            # Try to compile with enhancements
            result = subprocess.run(
                ["make", "-C", "../src", "clean"],
                capture_output=True, text=True, timeout=30
            )
            
            result = subprocess.run(
                ["make", "-C", "../src", "-j", "4"],
                capture_output=True, text=True, timeout=120
            )
            
            if result.returncode == 0:
                execution_time = time.time() - start_time
                return TestResult("compilation", True, execution_time, "Compilation successful")
            else:
                execution_time = time.time() - start_time
                return TestResult("compilation", False, execution_time, 
                                f"Compilation failed: {result.stderr}")
                
        except subprocess.TimeoutExpired:
            return TestResult("compilation", False, 120.0, "Compilation timeout")
        except Exception as e:
            return TestResult("compilation", False, 0.0, f"Compilation error: {str(e)}")
    
    def test_basic_functionality(self):
        """Test basic UCI functionality"""
        print("Testing basic functionality...")
        start_time = time.time()
        
        try:
            # Test UCI protocol
            process = subprocess.Popen([self.stockfish_path], 
                                     stdin=subprocess.PIPE, 
                                     stdout=subprocess.PIPE, 
                                     stderr=subprocess.PIPE, 
                                     text=True)
            
            # Send UCI commands
            stdout, stderr = process.communicate(input="uci\nisready\nquit\n", timeout=10)
            
            if "uciok" in stdout and "readyok" in stdout:
                execution_time = time.time() - start_time
                self.test_results.append(TestResult("basic_functionality", True, execution_time, 
                                                  "UCI protocol working"))
            else:
                execution_time = time.time() - start_time
                self.test_results.append(TestResult("basic_functionality", False, execution_time, 
                                                  "UCI protocol failed"))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("basic_functionality", False, execution_time, 
                                              f"Basic functionality error: {str(e)}"))
    
    def test_enhanced_search(self):
        """Test enhanced search algorithms"""
        print("Testing enhanced search algorithms...")
        start_time = time.time()
        
        test_positions = [
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # Starting position
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",  # Complex position
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"  # Endgame position
        ]
        
        try:
            with chess.engine.SimpleEngine.popen_uci(self.stockfish_path) as engine:
                engine.configure({"Threads": 1, "Hash": 64})
                
                total_score = 0
                for i, fen in enumerate(test_positions):
                    board = chess.Board(fen)
                    
                    # Test search depth and time
                    result = engine.analyse(board, chess.engine.Limit(depth=10, time=2.0))
                    
                    if result.get("score") and result.get("pv"):
                        total_score += 1
                        
                execution_time = time.time() - start_time
                success_rate = total_score / len(test_positions)
                
                self.test_results.append(TestResult("enhanced_search", success_rate > 0.8, 
                                                  execution_time, 
                                                  f"Search success rate: {success_rate:.2f}",
                                                  success_rate))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("enhanced_search", False, execution_time, 
                                              f"Enhanced search error: {str(e)}"))
    
    def test_enhanced_evaluation(self):
        """Test enhanced evaluation features"""
        print("Testing enhanced evaluation...")
        start_time = time.time()
        
        # Positions to test evaluation quality
        evaluation_tests = [
            ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0, 50),  # Starting position should be ~equal
            ("rnbqkb1r/pppp1ppp/5n2/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR w KQkq - 0 1", 50, 150),  # White advantage
            ("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R b KQkq - 0 1", -100, 100),  # Roughly equal
        ]
        
        try:
            with chess.engine.SimpleEngine.popen_uci(self.stockfish_path) as engine:
                engine.configure({"Threads": 1, "Hash": 32})
                
                correct_evaluations = 0
                for fen, min_cp, max_cp in evaluation_tests:
                    board = chess.Board(fen)
                    result = engine.analyse(board, chess.engine.Limit(depth=8))
                    
                    if result.get("score"):
                        score_cp = result["score"].white().score(mate_score=10000)
                        if score_cp and min_cp <= score_cp <= max_cp:
                            correct_evaluations += 1
                
                execution_time = time.time() - start_time
                accuracy = correct_evaluations / len(evaluation_tests)
                
                self.test_results.append(TestResult("enhanced_evaluation", accuracy > 0.6, 
                                                  execution_time, 
                                                  f"Evaluation accuracy: {accuracy:.2f}",
                                                  accuracy))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("enhanced_evaluation", False, execution_time, 
                                              f"Enhanced evaluation error: {str(e)}"))
    
    def test_time_management(self):
        """Test enhanced time management"""
        print("Testing time management...")
        start_time = time.time()
        
        try:
            with chess.engine.SimpleEngine.popen_uci(self.stockfish_path) as engine:
                engine.configure({"Threads": 1, "Hash": 32})
                
                board = chess.Board()
                
                # Test different time controls
                time_controls = [
                    chess.engine.Limit(time=1.0),
                    chess.engine.Limit(time=0.1),
                    chess.engine.Limit(depth=5),
                ]
                
                time_accuracy_scores = []
                for limit in time_controls:
                    search_start = time.time()
                    result = engine.analyse(board, limit)
                    actual_time = time.time() - search_start
                    
                    if limit.time:
                        # Check if actual time is close to allocated time
                        time_accuracy = min(1.0, limit.time / max(actual_time, 0.01))
                        time_accuracy_scores.append(time_accuracy)
                
                execution_time = time.time() - start_time
                avg_accuracy = sum(time_accuracy_scores) / len(time_accuracy_scores) if time_accuracy_scores else 0
                
                self.test_results.append(TestResult("time_management", avg_accuracy > 0.7, 
                                                  execution_time, 
                                                  f"Time management accuracy: {avg_accuracy:.2f}",
                                                  avg_accuracy))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("time_management", False, execution_time, 
                                              f"Time management error: {str(e)}"))
    
    def test_performance_benchmarks(self):
        """Test performance improvements"""
        print("Testing performance benchmarks...")
        start_time = time.time()
        
        try:
            # Run perft tests for performance measurement
            result = subprocess.run([self.stockfish_path], 
                                  input="position startpos\ngo perft 5\nquit\n",
                                  capture_output=True, text=True, timeout=30)
            
            if "nodes" in result.stdout:
                # Extract nodes per second if available
                lines = result.stdout.split('\n')
                nps_found = False
                for line in lines:
                    if "nodes" in line.lower() and "second" in line.lower():
                        nps_found = True
                        break
                
                execution_time = time.time() - start_time
                self.test_results.append(TestResult("performance_benchmark", True, 
                                                  execution_time, 
                                                  "Perft test completed successfully"))
            else:
                execution_time = time.time() - start_time
                self.test_results.append(TestResult("performance_benchmark", False, 
                                                  execution_time, 
                                                  "Perft test failed"))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("performance_benchmark", False, 
                                              execution_time, 
                                              f"Performance benchmark error: {str(e)}"))
    
    def test_tactical_positions(self):
        """Test tactical problem solving"""
        print("Testing tactical positions...")
        start_time = time.time()
        
        # Famous tactical positions with known best moves
        tactical_tests = [
            ("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 1", ["Bxf7+"]),  # Legal's mate
            ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", ["Qxf6"]),  # Tactical shot
        ]
        
        try:
            with chess.engine.SimpleEngine.popen_uci(self.stockfish_path) as engine:
                engine.configure({"Threads": 1, "Hash": 64})
                
                solved = 0
                for fen, best_moves in tactical_tests:
                    board = chess.Board(fen)
                    result = engine.analyse(board, chess.engine.Limit(depth=12, time=3.0))
                    
                    if result.get("pv") and len(result["pv"]) > 0:
                        best_move = result["pv"][0]
                        if str(best_move) in best_moves or best_move.uci() in [chess.Move.from_uci(m.replace("+", "")).uci() for m in best_moves]:
                            solved += 1
                
                execution_time = time.time() - start_time
                success_rate = solved / len(tactical_tests)
                
                self.test_results.append(TestResult("tactical_positions", success_rate > 0.5, 
                                                  execution_time, 
                                                  f"Tactical success rate: {success_rate:.2f}",
                                                  success_rate))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("tactical_positions", False, execution_time, 
                                              f"Tactical positions error: {str(e)}"))
    
    def test_endgame_positions(self):
        """Test endgame knowledge"""
        print("Testing endgame positions...")
        start_time = time.time()
        
        # Basic endgame positions
        endgame_tests = [
            ("8/8/8/8/8/8/8/K6k w - - 0 1", False),  # K vs K draw
            ("8/8/8/8/8/8/1K6/k6Q w - - 0 1", True),  # KQ vs K win
            ("8/8/8/8/8/8/1K6/k6R w - - 0 1", True),  # KR vs K win
        ]
        
        try:
            with chess.engine.SimpleEngine.popen_uci(self.stockfish_path) as engine:
                engine.configure({"Threads": 1, "Hash": 32})
                
                correct_assessments = 0
                for fen, should_be_winning in endgame_tests:
                    board = chess.Board(fen)
                    result = engine.analyse(board, chess.engine.Limit(depth=10))
                    
                    if result.get("score"):
                        score = result["score"].white().score(mate_score=10000)
                        is_winning = score and abs(score) > 300
                        
                        if is_winning == should_be_winning:
                            correct_assessments += 1
                
                execution_time = time.time() - start_time
                accuracy = correct_assessments / len(endgame_tests)
                
                self.test_results.append(TestResult("endgame_positions", accuracy > 0.6, 
                                                  execution_time, 
                                                  f"Endgame accuracy: {accuracy:.2f}",
                                                  accuracy))
                
        except Exception as e:
            execution_time = time.time() - start_time
            self.test_results.append(TestResult("endgame_positions", False, execution_time, 
                                              f"Endgame positions error: {str(e)}"))
    
    def generate_report(self) -> Dict[str, any]:
        """Generate comprehensive test report"""
        total_tests = len(self.test_results)
        passed_tests = sum(1 for result in self.test_results if result.passed)
        total_time = sum(result.execution_time for result in self.test_results)
        
        # Calculate weighted score based on test importance
        test_weights = {
            "compilation": 3.0,
            "basic_functionality": 3.0,
            "enhanced_search": 2.0,
            "enhanced_evaluation": 2.0,
            "time_management": 1.5,
            "performance_benchmark": 1.0,
            "tactical_positions": 2.0,
            "endgame_positions": 1.5
        }
        
        weighted_score = 0
        total_weight = 0
        for result in self.test_results:
            weight = test_weights.get(result.test_name, 1.0)
            total_weight += weight
            if result.passed:
                weighted_score += weight * (result.score if result.score > 0 else 1.0)
        
        overall_score = (weighted_score / total_weight) * 100 if total_weight > 0 else 0
        
        print("\n" + "=" * 60)
        print("STOCKFISH ENHANCEMENT TEST REPORT")
        print("=" * 60)
        print(f"Total tests run: {total_tests}")
        print(f"Tests passed: {passed_tests}")
        print(f"Success rate: {(passed_tests / total_tests) * 100:.1f}%")
        print(f"Total execution time: {total_time:.2f} seconds")
        print(f"Overall enhancement score: {overall_score:.1f}/100")
        print()
        
        print("Individual Test Results:")
        print("-" * 40)
        for result in self.test_results:
            status = "PASS" if result.passed else "FAIL"
            print(f"{result.test_name:25} {status:4} {result.execution_time:6.2f}s - {result.details}")
        
        if overall_score >= 80:
            print(f"\n🎉 EXCELLENT: Stockfish enhancements are working very well!")
        elif overall_score >= 60:
            print(f"\n✅ GOOD: Stockfish enhancements are working properly.")
        elif overall_score >= 40:
            print(f"\n⚠️  FAIR: Stockfish enhancements have some issues.")
        else:
            print(f"\n❌ POOR: Stockfish enhancements need significant work.")
        
        return {
            "total_tests": total_tests,
            "passed_tests": passed_tests,
            "success_rate": (passed_tests / total_tests) * 100,
            "overall_score": overall_score,
            "total_time": total_time,
            "individual_results": [
                {
                    "test": result.test_name,
                    "passed": result.passed,
                    "time": result.execution_time,
                    "details": result.details,
                    "score": result.score
                }
                for result in self.test_results
            ],
            "compilation_failed": False
        }

def main():
    """Main entry point for the test suite"""
    if len(sys.argv) > 1:
        stockfish_path = sys.argv[1]
    else:
        stockfish_path = "../src/stockfish"
    
    tester = StockfishEnhancementTester(stockfish_path)
    results = tester.run_all_tests()
    
    # Save results to JSON file
    with open("enhancement_test_results.json", "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"\nDetailed results saved to: enhancement_test_results.json")
    
    # Return appropriate exit code
    if results.get("compilation_failed", False):
        sys.exit(2)  # Compilation failure
    elif results["success_rate"] < 50:
        sys.exit(1)  # Test failures
    else:
        sys.exit(0)  # Success

if __name__ == "__main__":
    main()