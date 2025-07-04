#ifndef COLOR_HPP
#define COLOR_HPP

#include <string>

// Namespace untuk mengelompokkan warna
namespace Color {
    // ANSI Escape Codes untuk warna teks
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";

    // Fungsi untuk membuat teks berwarna
    inline std::string makeColor(const std::string& text, const std::string& color) {
        return color + text + RESET;
    }
}

#endif // COLOR_HPP
