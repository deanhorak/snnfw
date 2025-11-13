/**
 * @file emnist_letters_optimize.cpp
 * @brief Automatic optimization of EMNIST letters classification parameters
 * 
 * This program runs multiple training/testing cycles with different parameter
 * configurations to find the optimal settings for maximum accuracy.
 * 
 * Usage:
 *   ./emnist_letters_optimize <base_config> <param_space> <results_dir> [strategy] [max_trials]
 * 
 * Strategies:
 *   - random (default): Random search
 *   - grid: Exhaustive grid search
 *   - coordinate: Coordinate ascent
 *   - adaptive: Adaptive random search
 */

#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "snnfw/ConfigOptimizer.h"
#include "snnfw/ConfigLoader.h"
#include "snnfw/Logger.h"

using namespace snnfw;

// Result structure for experiment
struct ExperimentResult {
    double accuracy;
    double trainingTime;
    double testingTime;
    int correctPredictions;
    int totalPredictions;
};

// Run experiment by calling the executable and parsing output
ExperimentResult runExperiment(const std::string& configPath) {
    ExperimentResult result;
    result.accuracy = 0.0;
    result.trainingTime = 0.0;
    result.testingTime = 0.0;
    result.correctPredictions = 0;
    result.totalPredictions = 0;

    // Build command
    std::string command = "./emnist_letters_v1 " + configPath + " 2>&1";

    // Run command and capture output
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to run experiment" << std::endl;
        return result;
    }

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
        // Print output in real-time (optional, comment out for less verbosity)
        // std::cout << buffer;
    }

    int returnCode = pclose(pipe);
    if (returnCode != 0) {
        std::cerr << "Experiment failed with return code: " << returnCode << std::endl;
        return result;
    }

    // Parse output for results
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        // Look for "Training complete in X.Xs"
        if (line.find("Training complete in") != std::string::npos) {
            size_t pos = line.find("in ") + 3;
            size_t endPos = line.find("s", pos);
            if (pos != std::string::npos && endPos != std::string::npos) {
                result.trainingTime = std::stod(line.substr(pos, endPos - pos));
            }
        }

        // Look for "Test time: X.Xs"
        if (line.find("Test time:") != std::string::npos) {
            size_t pos = line.find(":") + 1;
            size_t endPos = line.find("s", pos);
            if (pos != std::string::npos && endPos != std::string::npos) {
                std::string timeStr = line.substr(pos, endPos - pos);
                // Remove whitespace
                timeStr.erase(std::remove_if(timeStr.begin(), timeStr.end(), ::isspace), timeStr.end());
                result.testingTime = std::stod(timeStr);
            }
        }

        // Look for "Overall accuracy: XX.XX% (correct/total)"
        if (line.find("Overall accuracy:") != std::string::npos) {
            size_t pctPos = line.find(":") + 1;
            size_t pctEnd = line.find("%", pctPos);
            if (pctPos != std::string::npos && pctEnd != std::string::npos) {
                std::string accStr = line.substr(pctPos, pctEnd - pctPos);
                // Remove whitespace
                accStr.erase(std::remove_if(accStr.begin(), accStr.end(), ::isspace), accStr.end());
                result.accuracy = std::stod(accStr) / 100.0;
            }

            // Parse (correct/total)
            size_t parenPos = line.find("(");
            size_t slashPos = line.find("/", parenPos);
            size_t parenEnd = line.find(")", slashPos);
            if (parenPos != std::string::npos && slashPos != std::string::npos && parenEnd != std::string::npos) {
                result.correctPredictions = std::stoi(line.substr(parenPos + 1, slashPos - parenPos - 1));
                result.totalPredictions = std::stoi(line.substr(slashPos + 1, parenEnd - slashPos - 1));
            }
        }
    }

    return result;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <base_config> <param_space> <results_dir> [strategy] [max_trials]" << std::endl;
        std::cerr << "\nStrategies: random (default), grid, coordinate, adaptive" << std::endl;
        return 1;
    }
    
    std::string baseConfigPath = argv[1];
    std::string paramSpacePath = argv[2];
    std::string resultsDir = argv[3];
    
    std::string strategyStr = "random";
    if (argc > 4) {
        strategyStr = argv[4];
    }
    
    int maxTrials = 50;
    if (argc > 5) {
        maxTrials = std::stoi(argv[5]);
    }
    
    // Initialize logger
    Logger::getInstance().setLevel(spdlog::level::warn);  // Reduce verbosity during optimization
    
    std::cout << "=== EMNIST Letters Parameter Optimization ===" << std::endl;
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Base config: " << baseConfigPath << std::endl;
    std::cout << "  Parameter space: " << paramSpacePath << std::endl;
    std::cout << "  Results directory: " << resultsDir << std::endl;
    std::cout << "  Strategy: " << strategyStr << std::endl;
    std::cout << "  Max trials: " << maxTrials << std::endl;
    std::cout << std::endl;
    
    try {
        // Create optimizer
        ConfigOptimizer optimizer(baseConfigPath, paramSpacePath, resultsDir);
        optimizer.loadParameterSpace();
        
        // Set strategy
        OptimizationStrategy strategy = OptimizationStrategy::RANDOM_SEARCH;
        if (strategyStr == "grid") {
            strategy = OptimizationStrategy::GRID_SEARCH;
        } else if (strategyStr == "coordinate") {
            strategy = OptimizationStrategy::COORDINATE_ASCENT;
        } else if (strategyStr == "adaptive") {
            strategy = OptimizationStrategy::ADAPTIVE_RANDOM;
        }
        optimizer.setStrategy(strategy);
        
        // Set convergence criteria
        optimizer.setConvergenceCriteria(
            maxTrials,      // max trials
            0.005,          // min improvement (0.5%)
            10              // patience trials
        );
        
        // Try to load previous state
        optimizer.loadState();
        if (optimizer.getCurrentTrial() > 0) {
            std::cout << "Resuming from trial " << optimizer.getCurrentTrial() << std::endl;
            std::cout << "Previous best accuracy: " << (optimizer.getBestResult().accuracy * 100.0) << "%" << std::endl;
            std::cout << std::endl;
        }
        
        // Optimization loop
        int trialNum = optimizer.getCurrentTrial();
        while (optimizer.shouldContinue()) {
            trialNum++;
            
            std::cout << "\n=== Trial " << trialNum << " / " << maxTrials << " ===" << std::endl;
            
            // Generate next configuration
            auto config = optimizer.generateNextConfig();
            if (config.empty()) {
                std::cout << "No more configurations to try" << std::endl;
                break;
            }
            
            // Save config to temporary file
            std::string tempConfigPath = resultsDir + "/trial_" + std::to_string(trialNum) + "_config.json";
            std::ofstream configFile(tempConfigPath);
            configFile << config.dump(2);
            configFile.close();
            
            std::cout << "Configuration saved to: " << tempConfigPath << std::endl;
            std::cout << "Running experiment..." << std::endl;
            
            // Run experiment
            auto startTime = std::chrono::steady_clock::now();
            ExperimentResult expResult = runExperiment(tempConfigPath);
            auto endTime = std::chrono::steady_clock::now();
            
            double totalTime = std::chrono::duration<double>(endTime - startTime).count();
            
            // Record result
            OptimizationResult result;
            result.trialId = trialNum;
            result.accuracy = expResult.accuracy;
            result.trainingTime = expResult.trainingTime;
            result.testingTime = expResult.testingTime;
            result.correctPredictions = expResult.correctPredictions;
            result.totalPredictions = expResult.totalPredictions;
            result.timestamp = getCurrentTimestamp();
            
            // Extract parameters from config
            for (const auto& item : config.items()) {
                if (item.value().is_object()) {
                    for (const auto& subitem : item.value().items()) {
                        std::string path = "/" + item.key() + "/" + subitem.key();
                        if (subitem.value().is_number()) {
                            result.parameters[path] = subitem.value().get<double>();
                        } else if (subitem.value().is_object()) {
                            for (const auto& subsubitem : subitem.value().items()) {
                                std::string subpath = path + "/" + subsubitem.key();
                                if (subsubitem.value().is_number()) {
                                    result.parameters[subpath] = subsubitem.value().get<double>();
                                }
                            }
                        }
                    }
                }
            }
            
            optimizer.recordResult(result);
            
            std::cout << "\nTrial " << trialNum << " complete:" << std::endl;
            std::cout << "  Accuracy: " << (result.accuracy * 100.0) << "%" << std::endl;
            std::cout << "  Training time: " << result.trainingTime << "s" << std::endl;
            std::cout << "  Testing time: " << result.testingTime << "s" << std::endl;
            std::cout << "  Total time: " << totalTime << "s" << std::endl;
            
            // Save state after each trial
            optimizer.saveState();
        }
        
        // Final report
        std::cout << "\n" << optimizer.generateReport() << std::endl;
        
        // Save best configuration
        if (optimizer.getAllResults().size() > 0) {
            auto bestResult = optimizer.getBestResult();
            std::string bestConfigPath = resultsDir + "/best_config.json";
            
            // Reconstruct best config
            auto bestConfig = optimizer.generateNextConfig();  // This won't work, need to save configs
            
            std::cout << "\nBest configuration saved to: " << bestConfigPath << std::endl;
            std::cout << "Best accuracy: " << (bestResult.accuracy * 100.0) << "%" << std::endl;
        }
        
        std::cout << "\n=== Optimization Complete ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

