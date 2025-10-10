#include <tgi.h>
#include <lynx.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>


#define SCREEN_W 160
#define SCREEN_H 102
#define NUM_STARS 75
#define MAX_DEPTH 64


extern unsigned char num_astros;
extern char astros_name[20][40];
extern char astros_craft[20][20];

const unsigned char star_pal[] = {	0x02, 0x00, 0x0D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x0F, 0x0F,
					                0x23, 0x00, 0x5F, 0x00, 0xD0, 0x0D, 0xAA, 0x00, 0x44, 0x66, 0x88, 0xAA, 0xCC, 0xEE, 0xFF, 0xFF}; 

typedef struct {
    char x;              // -80..79
    char y;              // -51..50
    unsigned char z;     // 1..64
} Star;

static Star stars[NUM_STARS];

char s[20];             // textout buffer
unsigned char astro;    // current astronaut being displayed


// Reset one star to the far plane
static void reset_star(Star *s) {
	s->x = (char)((rand() % SCREEN_W) - (SCREEN_W / 2));
	s->y = (char)((rand() % SCREEN_H) - (SCREEN_H / 2));

    s->z = MAX_DEPTH;
}

// Initialize all stars
static void init_stars(void) {
    unsigned char i;

    for (i = 0; i < NUM_STARS; ++i) {
	stars[i].x = (char)((rand() % SCREEN_W) - (SCREEN_W / 2));
	stars[i].y = (char)((rand() % SCREEN_H) - (SCREEN_H / 2));
        stars[i].z = (rand() % MAX_DEPTH) + 1;
    }
}

// Update draw the starfield
static void update_stars(void) {
    unsigned char i;
    unsigned char shade;
    signed int px, py;

    tgi_clear();

    for (i = 0; i < NUM_STARS; ++i) {
        Star *s = &stars[i];

        // Move star forward
        if (s->z > 0)
            s->z--;
        else {
            reset_star(s);
            continue;
        }

	// Perspective calcuation
	px = ((int)((signed char)s->x) * 50) / s->z + (SCREEN_W / 2);
	py = ((int)((signed char)s->y) * 50) / s->z + (SCREEN_H / 2);
        
	// Offscreen? recycle
        if ((px < 0) || (px >= SCREEN_W) || (py < 0) || (py >= SCREEN_H)) {
            reset_star(s);
            continue;
        }

        // --- Grayscale shading ---
        // z=64 dark gray (~8)
        // z=0  white (~15)
        shade = 15 - (s->z >> 3);  // divide by 8 for 0�8 range
        if (shade < 8) shade = 8;  // clamp lower end

        tgi_setcolor(shade);
        tgi_setpixel((unsigned char) px, (unsigned char) py);
    }

	// Display astronauts name and craft
	tgi_setcolor(2);
    sprintf(s, "%d people in space", num_astros);
	tgi_outtextxy(8, 1, s);

	tgi_setcolor((astro % 5)+2);
    tgi_outtextxy(80-((strlen(astros_name[astro])/2)*8), 46, astros_name[astro]);   
    tgi_outtextxy(80-((strlen(astros_craft[astro])/2)*8), 54, astros_craft[astro]);

    // Page flip for double buffering
    //tgi_setviewpage(draw_page);
    //draw_page ^= 1;
    //tgi_setdrawpage(draw_page);
	tgi_updatedisplay();

}


// Simple delay function
void delay(unsigned int n) {
    while (n--) {
    }
}


// Main
void display_astros(void) {
    unsigned long t;

    
    tgi_clear();
    tgi_setpalette(&star_pal);

    init_stars();
    astro = 0;

    t = clock();
    while (1) {
        // Display next Astronaut?
        if (((clock() - t) / CLOCKS_PER_SEC) > 3) {
            astro++;
            if (astro == num_astros)
                astro = 0;
            t = clock();
        }

        update_stars();
        while (tgi_busy());

        if (kbhit()) {
            cgetc();
            break;
        }
    }

    // set TGI back to normal
    tgi_setpalette(tgi_getdefpalette());
    tgi_setviewpage(0);
    tgi_setdrawpage(0);
}
