/**
 * Test program to debug saccades implementation
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include "snnfw/EMNISTLoader.h"

struct FixationRegion {
    std::string name;
    int rowStart, rowEnd, colStart, colEnd;
};

std::vector<uint8_t> extractFixationRegion(const std::vector<uint8_t>& imagePixels,
                                           const FixationRegion& region) {
    std::vector<uint8_t> regionPixels(28 * 28, 0);
    for (int y = region.rowStart; y <= region.rowEnd && y < 28; ++y) {
        for (int x = region.colStart; x <= region.colEnd && x < 28; ++x) {
            regionPixels[y * 28 + x] = imagePixels[y * 28 + x];
        }
    }
    return regionPixels;
}

int countNonZero(const std::vector<uint8_t>& pixels) {
    int count = 0;
    for (auto p : pixels) if (p > 0) count++;
    return count;
}

int main() {
    std::cout << "=== Saccades Debug Test ===" << std::endl;
    
    snnfw::EMNISTLoader loader(snnfw::EMNISTLoader::Variant::LETTERS);
    
    if (!loader.load("/home/dean/repos/ctm/data/raw/emnist-letters-train-images-idx3-ubyte",
                     "/home/dean/repos/ctm/data/raw/emnist-letters-train-labels-idx1-ubyte",
                     100, true)) {
        std::cerr << "Failed to load" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << loader.size() << " images" << std::endl;
    
    std::vector<FixationRegion> regions = {
        {"top", 0, 13, 0, 27},
        {"bottom", 14, 27, 0, 27},
        {"center", 7, 20, 7, 20},
        {"full", 0, 27, 0, 27}
    };
    
    for (int imgIdx = 0; imgIdx < 5; ++imgIdx) {
        auto& img = loader.getImage(imgIdx);
        int fullNonZero = countNonZero(img.pixels);
        
        std::cout << "\nImage " << imgIdx << " (" << img.getCharLabel() << "): "
                  << fullNonZero << " pixels" << std::endl;
        
        for (auto& region : regions) {
            auto fixPixels = extractFixationRegion(img.pixels, region);
            int nonZero = countNonZero(fixPixels);
            double pct = (double)nonZero / fullNonZero * 100.0;
            std::cout << "  " << region.name << ": " << nonZero << " pixels (" << pct << "%)" << std::endl;
        }
    }
    
    return 0;
}
