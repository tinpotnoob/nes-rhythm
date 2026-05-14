/*
github.com/tinpotnoob/nes-rhythm
rhythm game with select, start, b, a
ddr clone #67
for use with 8-bitworkshop

essentially arrows fall from the sky and you have to "catch them"
	uses conditionals to see if timing is right
*/

// TODO: optimizations:
// Do not use local variables, use static variables
// Inline any functions involved in main game loop

#include <stdlib.h>
#include <string.h>

// include NESLIB header
#include "neslib.h"

// include CC65 NES Header (PPU)
#include <nes.h>

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"


#include "graphics_info.h"

// link the pattern table into CHR ROM
//#link "resourcepack.s"

// Music
#include "music_player.h"
//#link "music_player.c"

#include "beatmap_player.h"
//#link "beatmap_player.c"


#include "apu.h"
//#link "apu.c"
#include "music_beethoven_virus.h"


///// DEFINES

#define COLS 30		// floor width in tiles
#define ROWS 60		// total scrollable height in tiles

#define GAPSIZE 4		// gap size in tiles

#define MAX_PLAYERS 2
#define SCREEN_Y_BOTTOM 240	// bottom of screen in pixels

#define SCROLL_VELOCITY 4	// Y velocity of arrows
#define TIMING_WINDOW 24	// Timing window for arrow presses in pixels
#define HIT_REWARD 20		// Score for each note
#define MISS_PENALTY 10		// Score for missing a note  -- NOT ACTIVE
#define BAD_TIMING_PENALTY 5	// Score for pressing key at wrong time
#define MAX_COMBO_REWARD 20
// Button coordinates
#define OFFSET_P2 15
#define RECEPTORS_Y 24
#define RECEPTORS_Y_PX 192
//24
#define SELECT_X 3
#define START_X 6
#define B_X 9
#define A_X 12

#define SELECT2_X (SELECT_X+OFFSET_P2)
#define START2_X (START_X+OFFSET_P2)
#define B2_X (B_X+OFFSET_P2)
#define A2_X (A_X+OFFSET_P2)
// Tile data
#define TILE_BLANK 0x00
#define TILE_SELECT 0xc4
#define TILE_START 0xc8
#define TILE_B 0xcc
#define TILE_A 0xd0
#define TILE_SELECT_OFF 0xd4
#define TILE_START_OFF 0xd8
#define TILE_B_OFF 0xdc
#define TILE_A_OFF 0xe0

//// METASPRITES
// define a 2x2 metasprite
#define DEF_METASPRITE_2x2(name,code,pal) \
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        8,      0,      (code)+2,   pal, \
        8,      8,      (code)+3,   pal, \
        128};

// define a 2x2 metasprite, flipped horizontally
#define DEF_METASPRITE_2x2_FLIP(name,code,pal) \
const unsigned char name[]={\
        8,      0,      (code)+0,   (pal)|OAM_FLIP_H, \
        8,      8,      (code)+1,   (pal)|OAM_FLIP_H, \
        0,      0,      (code)+2,   (pal)|OAM_FLIP_H, \
        0,      8,      (code)+3,   (pal)|OAM_FLIP_H, \
        128};

DEF_METASPRITE_2x2(ARROW_SELECT,TILE_SELECT,0)
DEF_METASPRITE_2x2(ARROW_START,TILE_START,0)
DEF_METASPRITE_2x2(ARROW_B,TILE_B,0)
DEF_METASPRITE_2x2(ARROW_A,TILE_A,0)
DEF_METASPRITE_2x2(ARROW_SELECT2,TILE_SELECT,1)
DEF_METASPRITE_2x2(ARROW_START2,TILE_START,1)
DEF_METASPRITE_2x2(ARROW_B2,TILE_B,1)
DEF_METASPRITE_2x2(ARROW_A2,TILE_A,1)

///ARROWS 
#define MAX_ARROWS 20
typedef struct {
    byte y[MAX_ARROWS];          // Y position (0–239)
    byte type[MAX_ARROWS];       // Arrow type (e.g., 0 = inactive, 1 = down, etc.)
} arrow_array;
static arrow_array arrows;
static byte active_arrows[MAX_ARROWS]; //indices to active arrows
static byte active_arrow_count;
///// STATIC VARIABLES
static signed long score[MAX_PLAYERS];
static unsigned int combo[MAX_PLAYERS];
static signed long high_score = 0;
static byte update_scoreboard;

static byte music_offset = 0;
static byte playback_speed = 1; // 0 for wait_nmi, 1 for wait_frame
static byte room_number;

static byte drawing_index; // for draw_arrows()
const byte HIGHLIGHT_STATE_ORIGINAL[8] = {0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55};
static byte highlight_state[8] = {0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55};
///// CONST VARIABLES
#define NUMBER_SONGS 1
//const char* SONG_NAMES = {"Beethoven Virus"};
const byte ARROW_COLUMNS[] = {0,SELECT_X*8,START_X*8,B_X*8,A_X*8,SELECT2_X*8,START2_X*8,B2_X*8,A2_X*8};
const unsigned char* const ARROW_SPRITES[] = {0,ARROW_SELECT,ARROW_START,ARROW_B,ARROW_A,ARROW_SELECT2,ARROW_START2,ARROW_B2,ARROW_A2};
const byte BUTTON_MAPPING[4] = {PAD_SELECT, PAD_START, PAD_B, PAD_A};
const byte lowerBound = RECEPTORS_Y_PX - TIMING_WINDOW; // For receptor timings
const byte upperBound = RECEPTORS_Y_PX + TIMING_WINDOW + 16;

#define MENU_OPTIONS 3
#define SONG_Y 10
#define RECORD_Y 12
#define SPEED_Y 15
#define OFFSET_Y 18
const byte OPTIONS_Y[MENU_OPTIONS] = {SONG_Y,SPEED_Y,OFFSET_Y};
///// ADDRESS FUNCTIONS

// return nametable address for tile (x,y)
// assuming vertical scrolling (horiz. mirroring)
/*
word getntaddr(byte x, byte y) {
  word addr;
  if (y < 30) {
    addr = NTADR_A(x,y);	// nametable A
  } else {
    addr = NTADR_C(x,y-30);	// nametable C
  }
  return addr;
}
*/
/*
// convert nametable address to attribute address
word nt2attraddr(word a) {
  return (a & 0x2c00) | 0x3c0 |
    ((a >> 4) & 0x38) | ((a >> 2) & 0x07);
}
*/

void print_scoreboard(byte player) {
  byte addrX;
  byte i;
  char buffer[11];
  itoa(score[player],buffer,10);
  for (i = strlen(buffer); i<11; i++) {
    buffer[i] = TILE_BLANK;
  }
  addrX = (!player) ? 2 : OFFSET_P2+2;
  vrambuf_put(NTADR_A(addrX,2),buffer,11);
  itoa(combo[player],buffer,10);
  for (i = strlen(buffer); i<11; i++) {
    buffer[i] = 0xa1;
  }
  addrX = (!player) ? 8 : OFFSET_P2+8;
  vrambuf_put(NTADR_A(addrX,RECEPTORS_Y+3),buffer,6);
}
// set up PPU
void setup_graphics(const char* palette, const char* attribute_table, const char* name_table) {
  ppu_off();
  oam_clear();
  pal_all(palette);
  vram_adr(0x2000);
  vram_write(name_table,960);
  vram_adr(0x23c0);
  vram_write(attribute_table, 0x40);
  vrambuf_clear();
  set_vram_update(updbuf);
  drawing_index = 0;
  ppu_on_all();
}

///// ARROW FUNCTIONS -- best things to optimize
void init_arrows() {
  byte i;
    for (i = 0; i < MAX_ARROWS; i++) {
        arrows.type[i] = 0;  // All arrows are inactive initially
        active_arrow_count = 0;
    }
}

void remove_active_arrow(byte index) {
  arrows.type[active_arrows[index]] = 0;
    if (index < active_arrow_count - 1) {
      active_arrows[index] = active_arrows[active_arrow_count - 1];
    }
  active_arrow_count--;
    }

void spawn_arrow(byte arrowType) {
  byte i;
      for (i = 0; i < MAX_ARROWS; i++) {
        if (!arrows.type[i]) {
            arrows.y[i] = 0;       // Start at the top of the screen
            arrows.type[i] = arrowType;  // Activate the arrow with specified type
            if (active_arrow_count < MAX_ARROWS) {
        	active_arrows[active_arrow_count++] = i;  // Add index to the active list
    		}
            break;
        }
    }
}

//returns what player
void scroll_arrows() {
  byte i;
    for (i = active_arrow_count; i > 0; i--) {
      	byte arrowIndex = active_arrows[i-1];
        byte* arrowY = &arrows.y[arrowIndex];
            *arrowY += SCROLL_VELOCITY;
            if (*arrowY > SCREEN_Y_BOTTOM) {
              byte* arrowType = &arrows.type[arrowIndex];
                  if (*arrowType <= 4) {
                  combo[0] = 0;
                  update_scoreboard |= 1;
                } else if (*arrowType <= 8) {
                  combo[1] = 0;
                  update_scoreboard |= 2;
                }
                remove_active_arrow(i-1);
           }
   }
}
                           
void draw_arrows() {
  byte i;
  byte numToDraw = (active_arrow_count < 16) ? active_arrow_count : 16;
  byte arrowIndex;
  byte oam_id = 0;
    for (i = 0; i < numToDraw; i++) {
      if (drawing_index > active_arrow_count - 1) {
        drawing_index = 0;
      }
      arrowIndex = active_arrows[drawing_index];
      oam_id = oam_meta_spr(ARROW_COLUMNS[arrows.type[arrowIndex]], arrows.y[arrowIndex], oam_id, ARROW_SPRITES[arrows.type[arrowIndex]]);
      drawing_index++;
    }
   if (oam_id != 0) oam_hide_rest(oam_id); // Hide unused sprites
}

void check_hits(byte buttonState, byte player) {
  byte i;
    const byte startIndex = (player<<2) + 1;  // 0 for player 1, 4 for player 2
    const byte endIndex = startIndex + 4;
    byte validPresses = 0; // Bitfield to track matched buttons
    byte arrowsToRemove[MAX_ARROWS];
    byte removeCount = 0;  // Counter for arrows to remove
    byte distance;
    byte comboReward;
    for (i = 0; i < active_arrow_count; i++) {
        byte arrowIndex = active_arrows[i];
        byte arrowY = arrows.y[arrowIndex];
        if (arrowY >= lowerBound && arrowY <= upperBound) {
            byte arrowType = arrows.type[arrowIndex];
            if (arrowType >= startIndex && arrowType < endIndex) {
                byte buttonIndex = arrowType - startIndex;
                byte buttonMask = BUTTON_MAPPING[buttonIndex];
                if ((buttonState & buttonMask) && !(validPresses & buttonMask)) {
                    distance = ((RECEPTORS_Y_PX) >= arrowY) ? ((RECEPTORS_Y_PX)-arrowY) : (arrowY-(RECEPTORS_Y_PX));
                    distance >>= 2;
                    comboReward = (++combo[player] >= 200) ? 20 : DIV10_LOOKUP[combo[player]];
                    // Mark the arrow as hit and track the button press
                    validPresses |= buttonMask;
                    arrowsToRemove[removeCount++] = i;
                    score[player] += (HIT_REWARD - distance) + (comboReward);
                }
            }
        }
    }
    // Remove marked arrows
    for (i = removeCount; i > 0; i--) {
        remove_active_arrow(arrowsToRemove[i - 1]);
    }

    // Penalize for invalid button presses
    for (i = 0; i < 4; i++) {
        byte buttonMask = BUTTON_MAPPING[i];
        if (buttonState & buttonMask) {
          if (!(validPresses & buttonMask)) {
            // Button was pressed but did not match a valid arrow
            score[player] -= BAD_TIMING_PENALTY;
            combo[player] = 0;
          }
        }
    }
}

#define highlight_buttons() { \
  if (pad1&PAD_SELECT) { \
    highlight_state[0] = 0x08; \
    highlight_state[1] |= 0x02; \
  } \
  if (pad1&PAD_START) { \
    highlight_state[1] |= 0x08; \
  } \
  if (pad1&PAD_B) { \
    highlight_state[2] = 0x0a; \
  } \
  if (pad1&PAD_A) { \
    highlight_state[3] = 0x02; \
  } \
  if (pad2&PAD_SELECT) { \
    highlight_state[4] = 0x59; \
  } \
  if (pad2&PAD_START) { \
    highlight_state[5] = 0x5a; \
  } \
  if (pad2&PAD_B) { \
    highlight_state[6] &= 0x5c; \
    highlight_state[6] |= 0x02; \
  } \
  if (pad2&PAD_A) { \
    highlight_state[6] &= 0x53; \
    highlight_state[6] |= 0x08; \
    highlight_state[7] = 0x56; \
  } \
}

#define pause() { \
  vrambuf_put(NTADR_A(10,13),"\35 to unpause",12); \
  vrambuf_put(NTADR_A(10,15),"\36 to restart",12); \
  vrambuf_put(NTADR_A(11,17),"\34 for main",10); \
  vrambuf_flush(); \
  while (1) { \
    pad1 = pad_trigger(0)|pad_trigger(1); \
    if (pad1&PAD_DOWN) { \
      break; \
    } else if (pad1&PAD_UP) { \
      room_number = 0; \
      return; \
    } else if (pad1&PAD_LEFT) { \
      room_number = 1; \
      return; \
    } \
  } \
  pad1 = 0; \
  vrambuf_put(NTADR_A(10,13),"\0\0\0\0\0\243\242\0\0\0\0\0",12); \
  vrambuf_put(NTADR_A(10,15),"\0\0\0\0\0\243\242\0\0\0\0\0",12); \
  vrambuf_put(NTADR_A(11,17),"\0\0\0\0\243\242\0\0\0\0",10); \
  vrambuf_flush(); \
}

#define check_inputs() { \
  pad1 = pad_trigger(0); \
  pad2 = pad_trigger(1); \
  if ((pad1|pad2)&PAD_DOWN) { \
    pad1 = 0; \
    pad2 = 0; \
    pause(); \
  } \
  if (pad1) { \
    check_hits(pad1,0); \
    update_scoreboard |= 1; \
  } \
  if (pad2) { \
    check_hits(pad2,1); \
    update_scoreboard |= 2; \
  } \
  highlight_buttons(); \
}

////ROOMS
void game_loop() {
  byte frameCounter = 0;
  byte nextArrow = 0;
  byte pad1 = 0;
  byte pad2 = 0;
  byte gameRunning = 180;
  update_scoreboard = 0;
  score[0] = 0;
  score[1] = 0;
  combo[0] = 0;
  combo[1] = 0;
  init_arrows();
  setup_graphics(PALETTE_GAME, ATTRIBUTE_TABLE_GAME, NAME_TABLE_GAME);
  //place_buttons();
  print_scoreboard(0);
  print_scoreboard(1);
  start_music(music_beethoven_virus,music_offset);
  start_beatmap(beatmap_beethoven_virus,restmap_beethoven_virus,sizeof(beatmap_beethoven_virus));
  while (gameRunning) {
   if (!is_music_active()) {
     gameRunning--;
   }
   if (is_beatmap_active()) {
     nextArrow = play_beatmap();
       if (nextArrow & (1)) spawn_arrow(1);
       if (nextArrow & (1<<1)) spawn_arrow(2);
       if (nextArrow & (1<<2)) spawn_arrow(3);
       if (nextArrow & (1<<3)) spawn_arrow(4);
       if (nextArrow & (1<<4)) spawn_arrow(5);
       if (nextArrow & (1<<5)) spawn_arrow(6);
       if (nextArrow & (1<<6)) spawn_arrow(7);
       if (nextArrow & (1<<7)) spawn_arrow(8);
   }
   play_music();
   check_inputs();
   scroll_arrows();
   draw_arrows();
   if (frameCounter > 2) {
     frameCounter = 0;
     if (update_scoreboard & 1) print_scoreboard(0);
     if (update_scoreboard & 2) print_scoreboard(1);
     update_scoreboard = 0;
     vrambuf_put(0x23f0,highlight_state,8);
        memcpy(highlight_state, HIGHLIGHT_STATE_ORIGINAL, sizeof(highlight_state));
   }
   if (!playback_speed) {
     ppu_wait_nmi();
   } else {
     ppu_wait_frame();
   }
   vrambuf_clear();
   frameCounter++;
  }
  room_number = 2; //game_over
}

void game_over() {
  byte pad;
  setup_graphics(PALETTE_GAME,ATTRIBUTE_TABLE_GAME,NAME_TABLE_GAME);
  print_scoreboard(0);
  print_scoreboard(1);
  ppu_off();
  vram_adr(NTADR_A(9,10));
  if (score[0] > score[1]) {
    vram_write("Player 1 wins!", 14);
    if (score[0] > high_score) {
      high_score = score[0];
      vram_adr(NTADR_A(11,12));
      vram_write("HIGH SCORE", 10);
    }
  } else if (score[0] < score[1]) {
    vram_write("Player 2 wins!", 14);
    if (score[1] > high_score) {
      high_score = score[1];
      vram_adr(NTADR_A(11,12));
      vram_write("HIGH SCORE", 10);
    }
  } else {
    vram_write("   \257 DRAW \257  ", 14);
    if (score[0] > high_score) {
      high_score = score[0];
      vram_adr(NTADR_A(11,12));
      vram_write("HIGH SCORE", 10);
    }
  }
  vram_adr(NTADR_A(10,15));
  vram_write("\36 to restart",12);
  vram_adr(NTADR_A(11,17));
  vram_write("\34 for main",10);
  ppu_on_all();
  while (1) {
	pad = pad_trigger(0)|pad_trigger(1);
        if (pad & PAD_UP) {
          room_number = 0;
          break;
        } else if (pad & PAD_LEFT) {
          room_number = 1;
          break;
        }
  };
  delay(30);
}
#define MAX_FRAME_DELAY 180
#define MIN_FRAME_DELAY 1
#define READ_OFFSETS
void set_music_offset() {
    // Array to hold delay counts for button presses
    byte currentFrameDelay = 48;
    byte newFrameDelay = currentFrameDelay; // ensures that frame delay doesnt change during an interation
    byte pad;
    byte frameCounter = 0;
  #ifdef READ_OFFSETS
    char buffer[4];
    byte i;
  #endif
    // Initialize graphics and place buttons
    setup_graphics(PALETTE_GAME, ATTRIBUTE_TABLE_GAME, NAME_TABLE_GAME);
    // Display instructions on the screen
    vrambuf_put(NTADR_A(8, 2), "Check music sync", 16);
    vrambuf_put(NTADR_A(8, 4), "UP/DOWN: adjust", 15);
    vrambuf_put(NTADR_A(9, 6), "SEL: continue", 13);
    vrambuf_flush();
    init_arrows();
    spawn_arrow(4);
    // Main loop for collecting delay data
    while (1) {
      pad = pad_trigger(0)|pad_trigger(1);
      if ((pad & PAD_UP) && (newFrameDelay < MAX_FRAME_DELAY)) newFrameDelay++;
      if ((pad & PAD_DOWN) && (newFrameDelay > MIN_FRAME_DELAY)) newFrameDelay--;
      if (pad & PAD_SELECT) break;
      scroll_arrows();
      draw_arrows();
      // Play sound afater frame delay, then respawn arrow
      if (frameCounter >= currentFrameDelay) {
        APU_TRIANGLE_LENGTH(107, 0);
        frameCounter = 0;
        currentFrameDelay = newFrameDelay; // change frame delay AFTER iteration
        spawn_arrow(4);
      }
       #ifdef READ_OFFSETS     
      itoa(currentFrameDelay,buffer,10);
        for (i = strlen(buffer); i<4; i++) {
           buffer[i] = TILE_BLANK;
         }
        vrambuf_put(NTADR_A(2,8),buffer,4);
        #endif
      ppu_wait_nmi();
      vrambuf_clear();
      frameCounter++;
   }
  music_offset = currentFrameDelay;
  delay(30);
  room_number = 0;
}

void main_menu() {
  char buffer[12];
  byte menuCursor = 0;
  byte lastCursor = 1;
  byte pad;
  byte i;
  
  setup_graphics(PALETTE_GAME,ATTRIBUTE_TABLE_GAME,NAME_TABLE_MENU);
  ppu_off();
  vram_adr(NTADR_A(17, SONG_Y));
  vram_write("Beethoven",9);
  vram_adr(NTADR_A(17,RECORD_Y));
  itoa(high_score,buffer,10);
  vram_write(buffer,strlen(buffer));
  vram_adr(NTADR_A(17,SPEED_Y));
  if (playback_speed) {
    vram_write("1",1);
  } else {
    vram_write("2",1); 
  }
  vram_adr(NTADR_A(17,OFFSET_Y));
  itoa(music_offset,buffer,10);
  vram_write(buffer,strlen(buffer));
  ppu_on_all();
  start_music(music_mii_channel,0);
  while (1) {
    if (!is_music_active()) start_music(music_mii_channel,0);
    play_music();
    pad = pad_trigger(0)|pad_trigger(1);
     if (pad&PAD_DOWN && menuCursor < MENU_OPTIONS-1) {
       menuCursor++;
    } else if (pad&PAD_UP && menuCursor > 0) {
       menuCursor--;
    }
    if (lastCursor != menuCursor) {
      vrambuf_put(NTADR_A(7,OPTIONS_Y[lastCursor]),"\0",1);
      vrambuf_put(NTADR_A(7,OPTIONS_Y[menuCursor]),"\30",1);
      lastCursor = menuCursor;
    }
    switch (menuCursor) {
      case 0:
        if (pad&PAD_A) {
          room_number = music_offset ? 1 : 3;
          return;
        }
        break;
      case 1:
        if (pad&PAD_LEFT) {
          vrambuf_put(NTADR_A(17,SPEED_Y),"1",1);
          playback_speed = 1;
        } else if (pad&PAD_RIGHT) {
          vrambuf_put(NTADR_A(17,SPEED_Y),"2",1);
          playback_speed = 0;
        }
        break;
      case 2:
        if (pad&PAD_A) {
          room_number = 3;
          return;
        } else if (pad&PAD_RIGHT && music_offset < MAX_FRAME_DELAY) {
          itoa(++music_offset,buffer,10);
            for (i = strlen(buffer); i<3; i++) {
                buffer[i] = TILE_BLANK;
            }
            vrambuf_put(NTADR_A(17,OFFSET_Y),buffer,3);
        } else if (pad&PAD_LEFT && music_offset > MIN_FRAME_DELAY) {
          itoa(--music_offset,buffer,10);
            for (i = strlen(buffer); i<3; i++) {
                 buffer[i] = TILE_BLANK;
            }
            vrambuf_put(NTADR_A(17,OFFSET_Y),buffer,3);
        }
        break;
      default:
        menuCursor = 0;
    }
    ppu_wait_nmi();
    vrambuf_clear();
  }
}

void main() {
  apu_init();
  room_number = 0; // main menu
  while (1) {
    switch (room_number) {
      case 0:
        main_menu();
        break;
      case 1:
        game_loop();
        break;
      case 2:
        game_over();
        break;
      case 3:
        set_music_offset();
        break;
      default:
        game_over();
   }
  }
}