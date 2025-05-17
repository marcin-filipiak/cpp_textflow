#include <ncurses.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <iomanip>
#include <sstream>

/**
 * Disable terminal software flow control (e.g. Ctrl+S and Ctrl+Q)
 */
void disableCtrlS() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_iflag &= ~IXON;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

/**
 * Load file contents into a vector of strings
 */
bool loadFromFile(const std::string& filename, std::vector<std::string>& lines) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return true;
}

/**
 * Save the text content to a file
 */
bool saveToFile(const std::string& filename, const std::vector<std::string>& lines) {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) return false;

    for (const auto& line : lines) {
        outfile << line << '\n';
    }
    return true;
}

/**
 * Draw text content with line numbers on the screen
 */
void drawText(const std::vector<std::string>& lines, int start_line, int height, int width) {
    const int max_digits = 4;
    for (int i = 0; i < height - 1 && i + start_line < lines.size(); ++i) {
        std::ostringstream oss;
        oss << std::setw(max_digits) << std::setfill(' ') << (i + start_line + 1);
        std::string formatted_line_number = oss.str();

        attron(COLOR_PAIR(3));
        mvprintw(i + 1, 0, "%s| ", formatted_line_number.c_str());
        attroff(COLOR_PAIR(3));

        mvprintw(i + 1, max_digits + 2, "%s", lines[i + start_line].c_str());
    }
}

/**
 * Draw the header bar with the application title
 */
void drawHeader(int width) {
    attron(COLOR_PAIR(4));
    for (int i = 0; i < width; ++i) {
        mvaddch(0, i, ' ');
    }
    mvprintw(0, (width - 8) / 2, "TextFlow");
    attroff(COLOR_PAIR(4));
}

/**
 * Adjust cursor position based on key input
 */
void moveCursor(int& cursor_y, int& cursor_x, int key, int height, int width, int& start_line, const std::vector<std::string>& lines) {
    if (key == KEY_DOWN) {
        if (cursor_y + start_line + 1 < lines.size()) {
            if (cursor_y < height - 2)
                cursor_y++;
            else
                start_line++;
            cursor_x = std::min(cursor_x, (int)lines[cursor_y + start_line].size());
        }
    } else if (key == KEY_UP) {
        if (cursor_y > 0)
            cursor_y--;
        else if (start_line > 0)
            start_line--;
        cursor_x = std::min(cursor_x, (int)lines[cursor_y + start_line].size());
    } else if (key == KEY_LEFT) {
        if (cursor_x > 0)
            cursor_x--;
    } else if (key == KEY_RIGHT) {
        if (cursor_x < lines[cursor_y + start_line].size())
            cursor_x++;
    }
    else if (key == KEY_NPAGE) {  // Page Down
        int lines_remaining = lines.size() - (start_line + cursor_y);
        int scroll = std::min(height - 2, lines_remaining - 1);
        start_line += scroll;
        cursor_y = 0;
        if (start_line + cursor_y >= (int)lines.size())
            start_line = lines.size() - height + 2;
        if (start_line < 0) start_line = 0;
        cursor_x = std::min(cursor_x, (int)lines[start_line + cursor_y].size());
    } else if (key == KEY_PPAGE) {  // Page Up
        int scroll = std::min(height - 2, start_line + cursor_y);
        start_line -= scroll;
        if (start_line < 0) {
            scroll += start_line;
            start_line = 0;
        }
        cursor_y = 0;
        cursor_x = std::min(cursor_x, (int)lines[start_line + cursor_y].size());
    }
    else if (key == KEY_HOME) { // home
        cursor_x = 0;
    } else if (key == KEY_END) { // end
        if (start_line + cursor_y < (int)lines.size())
            cursor_x = lines[start_line + cursor_y].size();
    }
}

/**
 * Edit text based on key input
 */
void editText(int key, int& cursor_y, int& cursor_x, int height, int width, std::vector<std::string>& lines, int& start_line) {
    auto& line = lines[cursor_y + start_line];

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursor_x > 0) {
            line.erase(cursor_x - 1, 1);
            cursor_x--;
        } else if (cursor_y + start_line > 0) {
            auto& prev_line = lines[cursor_y + start_line - 1];
            cursor_x = prev_line.size();
            prev_line += line;
            lines.erase(lines.begin() + cursor_y + start_line);
            cursor_y--;
        }
    } else if (key >= 32 && key <= 126) {
        line.insert(cursor_x, 1, static_cast<char>(key));
        cursor_x++;
    } else if (key == 10) {  // Enter
        std::string new_line = line.substr(cursor_x);
        line = line.substr(0, cursor_x);
        lines.insert(lines.begin() + cursor_y + start_line + 1, new_line);
        cursor_y++;
        cursor_x = 0;

        if (cursor_y >= height - 2) {
            cursor_y = height - 2;
            start_line++;
        }
    } else if (key == KEY_DC) {
        if (cursor_x < line.size()) {
            line.erase(cursor_x, 1);
        } else if (cursor_y + start_line + 1 < lines.size()) {
            line += lines[cursor_y + start_line + 1];
            lines.erase(lines.begin() + cursor_y + start_line + 1);
        }
    }
}

/**
 * Set color for info/error message
 */
void setMessageColor(bool success) {
    attron(COLOR_PAIR(success ? 1 : 2));
}

/**
 * Display usage info
 */
void printUsage(const char* programName) {
    std::cout << "Program: TextFlow\n";
    std::cout << "Author: Marcin Filipiak\n";
    std::cout << "Description: This program processes the file given as an argument.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " <filename>\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << programName << " data.txt\n";
}

/**
 * Display status/info message
 */
void showMessage(const std::string& message, int height, bool success) {
    setMessageColor(success);
    mvprintw(height - 1, 0, "%s", message.c_str());
    refresh();
    napms(1000);
    attroff(COLOR_PAIR(1));
    attroff(COLOR_PAIR(2));
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> lines;
    if (!loadFromFile(argv[1], lines)) {
        std::cerr << "Cannot open file: " << argv[1] << std::endl;
        return 1;
    }

    disableCtrlS();

    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_WHITE);
    init_pair(2, COLOR_RED, COLOR_WHITE);
    init_pair(3, COLOR_CYAN, COLOR_BLACK);
    init_pair(4, COLOR_BLACK, COLOR_WHITE);

    int height, width;
    getmaxyx(stdscr, height, width);

    int start_line = 0;
    int cursor_y = 1;
    int cursor_x = 0;
    const int max_digits = 4;

    while (true) {
        clear();
        drawHeader(width);
        drawText(lines, start_line, height, width);
        move(cursor_y + 1, cursor_x + max_digits + 2);
        refresh();

        int ch = getch();

        if (ch == 24) break; // Ctrl+X = exit

        if (ch == 19) { // Ctrl+S = save
            bool ok = saveToFile(argv[1], lines);
            showMessage(ok ? "File saved successfully." : "Failed to save the file!", height, ok);
        } else {
            moveCursor(cursor_y, cursor_x, ch, height, width, start_line, lines);
            editText(ch, cursor_y, cursor_x, height, width, lines, start_line);
        }
    }

    endwin();
    return 0;
}
