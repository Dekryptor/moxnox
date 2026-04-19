#include <curses.h>

int main() {
    initscr();
    cbreak();             // Line buffering disabled
    noecho();             // Don't echo() while we do getch
    curs_set(0);          // Hide the cursor

    int height = 10, width = 30, start_y = 5, start_x = 5;

    // Create a new window for the box
    WINDOW *win = newwin(height, width, start_y, start_x);
    wrefresh(stdscr);            // Refresh stdscr so win appears on top

    // 0,0 uses default characters for the border lines
    wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);

    // Print centered text inside the box
    mvwprintw(win, height/2, 2, "Ncurses Box Test");
    wrefresh(win);        // Refresh only the window

    getch();              // Wait for user input
    endwin();
    return 0;
}
