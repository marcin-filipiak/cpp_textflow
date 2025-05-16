#include <ncurses.h>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <iomanip>
#include <sstream>

// Disable flow control (Ctrl+S)
void disableCtrlS() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_iflag &= ~IXON;  // Disable XON/XOFF
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Function to save content to a file
bool saveToFile(const std::string& filename, const std::vector<std::string>& lines) {
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        for (const auto& line : lines) {
            outfile << line << '\n';
        }
        return true;
    }
    return false;
}

// Function to draw text on the screen
void drawText(const std::vector<std::string>& lines, int start_line, int height, int width) {
    const int max_digits = 4; // Fixed width for line numbering
    for (int i = 0; i < height && i + start_line < lines.size(); ++i) {
        std::ostringstream oss;
        oss << std::setw(max_digits) << std::setfill(' ') << (i + start_line + 1);
        std::string formatted_line_number = oss.str();

        attron(COLOR_PAIR(2)); // Gray color
        
        std::string line_separator = u8"|"; // UTF-8 vertical line character
        mvprintw(i, 0, "%s%s ", formatted_line_number.c_str(), line_separator.c_str());
        attroff(COLOR_PAIR(3));

        mvprintw(i, max_digits + 2, "%s", lines[i + start_line].c_str());
    }
}

// Function to manage cursor movement
void moveCursor(int& cursor_y, int& cursor_x, int key, int height, int width, int& start_line, const std::vector<std::string>& lines) {
    if (key == KEY_DOWN) {
        if (cursor_y + start_line + 1 < (int)lines.size()) {
            if (cursor_y < height - 1)
                cursor_y++;
            else
                start_line++;
            if (cursor_x > (int)lines[cursor_y + start_line].size())
                cursor_x = lines[cursor_y + start_line].size();
        }
    } else if (key == KEY_UP) {
        if (cursor_y > 0)
            cursor_y--;
        else if (start_line > 0)
            start_line--;
        if (cursor_x > (int)lines[cursor_y + start_line].size())
            cursor_x = lines[cursor_y + start_line].size();
    } else if (key == KEY_LEFT) {
        if (cursor_x > 0)
            cursor_x--;
    } else if (key == KEY_RIGHT) {
        if (cursor_x < (int)lines[cursor_y + start_line].size())
            cursor_x++;
    }
}

// Function to handle text editing
void editText(int key, int& cursor_y, int& cursor_x, int height, int width, std::vector<std::string>& lines, int start_line) {
    auto& line = lines[cursor_y + start_line];

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursor_x > 0 && !line.empty()) {
            // If the cursor is not at the beginning of the line, delete a single character
            line.erase(cursor_x - 1, 1);
            cursor_x--;
        } else if (cursor_y + start_line > 0) {
            // If the cursor is at the beginning of the line and not at the first line
            // Merge the current line with the previous one
            auto& prev_line = lines[cursor_y + start_line - 1];
            prev_line += line;  // Append the current line's content to the previous line
            lines.erase(lines.begin() + cursor_y + start_line);  // Remove the current line

            cursor_y--;  // Move the cursor to the previous line
            cursor_x = prev_line.size();  // Set the cursor at the end of the previous line
        }
    } else if (key >= 32 && key <= 126) {
        // Handle character input
        if ((int)line.size() < width - 1) {
            line.insert(cursor_x, 1, (char)key);
            cursor_x++;
        }
    } else if (key == 10) {  // Handle Enter (newline)
        if (cursor_y + start_line <= lines.size() - 1) {
            std::string new_line = line.substr(cursor_x);
            line = line.substr(0, cursor_x);
            lines.insert(lines.begin() + cursor_y + start_line + 1, new_line);

            cursor_y++;
            cursor_x = 0;

            if (cursor_y >= height - 1) {
                cursor_y = height - 1;
                start_line++;
            }
        }
    }
    else if (key == KEY_DC) {
    if (cursor_x < (int)line.size()) {
        // Delete character under cursor
        line.erase(cursor_x, 1);
    } else if (cursor_y + start_line + 1 < (int)lines.size()) {
        // Join current line with the next line
        line += lines[cursor_y + start_line + 1];
        lines.erase(lines.begin() + cursor_y + start_line + 1);
    }
}
}

// Function to set message color
void setMessageColor(bool success) {
    if (success) {
        attron(COLOR_PAIR(1));  // Green
    } else {
        attron(COLOR_PAIR(2));  // Red
    }
}

// Function to display a message
void showMessage(const std::string& message, int height, bool success) {
    setMessageColor(success);
    mvprintw(height - 1, 0, "%s", message.c_str());
    refresh();
    napms(1000); // Wait 1 second
    attroff(COLOR_PAIR(1));
    attroff(COLOR_PAIR(2));
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << argv[1] << std::endl;
        return 1;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    disableCtrlS();

    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_WHITE, COLOR_BLACK);

    int height, width;
    getmaxyx(stdscr, height, width);

    int start_line = 0;
    int cursor_y = 0;
    int cursor_x = 0;

    const int max_digits = 4;

    int ch;

    while (true) {
        clear();
        drawText(lines, start_line, height, width);
        move(cursor_y, cursor_x + max_digits + 2);
        refresh();

        ch = getch();

        if (ch == 24) { // Ctrl+X
            break;
        } else if (ch == 19) { // Ctrl+S
            if (saveToFile(argv[1], lines)) {
                showMessage("File saved successfully.", height, true);
            } else {
                showMessage("Failed to save the file!", height, false);
            }
        } else {
            moveCursor(cursor_y, cursor_x, ch, height, width, start_line, lines);
            editText(ch, cursor_y, cursor_x, height, width, lines, start_line);
        }
    }

    endwin();
    return 0;
}

