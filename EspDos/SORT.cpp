#include <Arduino.h>
#include "DOS.h"
#include "USERIO.h"
#include <algorithm>

// Need to expose readLinesFromStdin helper from USERIO.H if I made it public static?
// I put it in USERIO.H as static inline function (implicit in header) but I should check.
// In previous step I put `static std::vector<String> readLinesFromStdin()` in `USERIO.H`.
// Since it is static in header, each CPP including it gets a copy. This is fine.

// --- SORT ---
int sort_main(int argc, char* argv[]) {
    // Read all lines from stdin, sort, print
    std::vector<String> lines = readLinesFromStdin();

    // Simple sort
    std::sort(lines.begin(), lines.end());

    if (argc > 1 && String(argv[1]) == "/R") {
        std::reverse(lines.begin(), lines.end());
    }

    for (const auto& line : lines) {
        DosIO::println(line);
    }

    return 0;
}
