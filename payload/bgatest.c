/* Little C (Dobby) demo script: BGA test
   Run from kernel prompt after 'make database' + 'make run':
   dobby bgatest.c
*/

int rgb(int r, int g, int b)
{
  /* 0x00RRGGBB */
  return r*65536 + g*256 + b;
}

main()
{
  int w, h;

  puts("BGA test start\n");
  
  if (bga_init(640, 480) != 0) {
    puts("bga_init failed\n");
    return 0;
  }

  w = bga_width();
  h = bga_height();

  /* Hintergrund */
  bga_clear(rgb(16,16,24));

  /* Rahmen in den Ecken (farbige Linien) */
  bga_drawline(0,0, w-1,0,     rgb(255, 0,   0));   /* oben rot */
  bga_drawline(w-1,0, w-1,h-1, rgb(0,   255, 0));   /* rechts grün */
  bga_drawline(w-1,h-1, 0,h-1, rgb(0,   0,   255)); /* unten blau */
  bga_drawline(0,h-1, 0,0,     rgb(255, 255, 0));   /* links gelb */

  /* Fadenkreuz */
  bga_drawline(0, h/2, w-1, h/2, rgb(200,200,200));
  bga_drawline(w/2, 0, w/2, h-1, rgb(200,200,200));

  /* Gefülltes Dreieck in der Mitte */
  bga_drawtri(w/2, 60, 60, h-60, w-60, h-60, rgb(0, 17, 255));

  /* Taste oder Timeout (5s) */
  puts("Press any key or wait 5s...\n");

	/* Warten auf Tastendruck oder Timeout */
	puts("Press any key or wait 5s...\n");
  {
    int waited; int done; int sc;
    waited = 0; done = 0;
    /* Eingabepuffer leeren */
    while (getkey_async()) { }
    /* Poll bis Taste oder Timeout */
    while (done == 0) {
      sc = getkey_async();
      if (sc != 0) {
        done = 1;
      } else {
        sleep(50);
        waited = waited + 50;
        if (waited >= 5000) done = 1;
      }
    }
  }

  /* Fallback: explizit Textmodus erzwingen */
  txtmode();
}
