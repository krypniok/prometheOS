/* Little C: BGA Pixel-Gradient-Demo
   Start im Prompt: dobby bga_gradient.c
*/

main()
{
  int w,h,x,y;
  if (bga_init(320,200)!=0) { puts("bga_init failed\n"); return 0; }
  w=bga_width(); h=bga_height();
  for (y=0;y<h;y=y+1){
    for (x=0;x<w;x=x+1){
      int r = x*255/(w-1);
      int g = y*255/(h-1);
      int b = (x+y)*255/(w+h-2);
      int c = r*65536 + g*256 + b;
      bga_drawpixel(x,y,c);
    }
  }
  puts("Press key...\n"); getche();
  bga_close();
}

