unsigned char snakestring[206] = "MovinMovinMovinMovin12345";
int ox = 0, oy = 0, x = 0, y = 0, old_x = 0, old_y = 0, old_x2 = 0, old_y2 = 0, bReverse = 0, bReverse2 = 0;

void nextposition() {
    old_x = x; old_y = y;
    if (!bReverse) {
        if (x < 79 && y == 0) { x++; }
        else if (x == 79 && y < 24) { y++; }
        else if (x == 79 && y == 24) { bReverse = 1; }
        else if (x == 0 && y > 0) { y--; } // Korrektur für oben rechts (79, 0)
    }
    else {
        if (x > 0 && y == 24) { x--; }
        else if (x == 0 && y > 0) { y--; }
        else if (x == 0 && y == 0) { bReverse = 0; }
        else if (x == 79 && y < 24) { y++; } // Korrektur für unten links (0, 24)
    }
}

void nextposition2() {
    old_x2 = ox; old_y2 = oy;
    if (!bReverse2) {
        if (ox < 79 && oy == 0) { ox++; }
        else if (ox == 79 && oy < 24) { oy++; }
        else if (ox == 79 && oy == 24) { bReverse2 = 1; }
        else if (ox == 0 && oy > 0) { oy--; } // Korrektur für oben rechts (79, 0)
    }
    else {
        if (ox > 0 && oy == 24) { ox--; }
        else if (ox == 0 && oy > 0) { oy--; }
        else if (ox == 0 && oy == 0) { bReverse2 = 0; }
        else if (ox == 79 && oy < 24) { oy++; } // Korrektur für unten links (0, 24)
    }
}

void sub_timer_snaketext_callback2() {
    int len = strlen(snakestring);
    printChar(old_x, old_y, 0x0F, ' ');
    printChar(old_x2, old_y2, 0x0F, ' ');
    x = ox; y = oy;
    for (int i = 0; i < len; i++) {
        printChar(x, y, 0x0F, snakestring[i]);
        nextposition();
    }
    nextposition2();
}

void sub_timer_snaketext_callback() {
    int len = strlen(snakestring);
    printChar(old_x, old_y, 0x0F, ' ');
    printChar(old_x2, old_y2, 0x0F, ' ');

    // Aktualisiere die Position von Text 1
    x = ox;
    y = oy;
    for (int i = 0; i < len; i++) {
        if (x >= 0 && x < 80 && y >= 0 && y < 25) {
            printChar(x, y, 0x0F, snakestring[i]);
        }
        nextposition();

        // Überprüfe, ob die Position außerhalb des sichtbaren Bereichs ist
        if (x < 0 || x >= 80 || y < 0 || y >= 25) {
            // Ändere die Richtung oder positioniere den Text neu
            if (x < 0) x = 0;
            if (x >= 80) x = 79;
            if (y < 0) y = 0;
            if (y >= 25) y = 24;
        }
    }

    // Aktualisiere die Position von Text 2
    nextposition2();
}

int snkaketext_threadid=0, snaketext_running=0;
void snaketext() {
   if(snaketext_running) {
      playDTMF("*31#0461#");
      remove_sub_timer(snkaketext_threadid);
      snaketext_running=0;
   } else {
      snkaketext_threadid = add_sub_timer(50, sub_timer_snaketext_callback);
      snaketext_running=1;
   }
}
