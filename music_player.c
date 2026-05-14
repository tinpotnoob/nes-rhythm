/*
A simple music player.
*/

#include <string.h>
#include <nes.h>

#include "neslib.h"

#include "apu.h"
//#link "apu.c"

//
// MUSIC ROUTINES
//

// Namespace(bias=1.0, freq=111860.8, length=64, maxbits=13.0, upper=41)
// 439.0 0.943191918851 41
const int note_table_41[64] = {
4318, 4076, 3847, 3631, 3427, 3235, 3053, 2882, 2720, 2567, 2423, 2287, 2159, 2037, 1923, 1815, 1713, 1617, 1526, 1440, 1360, 1283, 1211, 1143, 1079, 1018, 961, 907, 856, 808, 763, 720, 679, 641, 605, 571, 539, 509, 480, 453, 428, 403, 381, 359, 339, 320, 302, 285, 269, 254, 240, 226, 213, 201, 190, 179, 169, 160, 151, 142, 134, 126, 119, 113, };

// Namespace(bias=1.0, freq=111860.8, length=64, maxbits=13, upper=49)
// 440.5 1.79281159771 49
const int note_table_49[64] = {
4304, 4062, 3834, 3619, 3416, 3224, 3043, 2872, 2711, 2559, 2415, 2279, 2151, 2031, 1917, 1809, 1707, 1611, 1521, 1436, 1355, 1279, 1207, 1139, 1075, 1015, 958, 904, 853, 805, 760, 717, 677, 639, 603, 569, 537, 507, 478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253, 239, 225, 213, 201, 189, 179, 168, 159, 150, 142, 134, 126, 119, 112, };

// Namespace(bias=1.0, freq=111860.8, length=64, maxbits=12, upper=63)
// 443.6 14.2328382554 63
const int note_table_63[64] = {
2137, 4034, 3807, 3593, 3392, 3201, 3022, 2852, 2692, 2541, 2398, 2263, 2136, 2016, 1903, 1796, 1695, 1600, 1510, 1425, 1345, 1270, 1199, 1131, 1068, 1008, 951, 898, 847, 800, 755, 712, 672, 634, 599, 565, 533, 503, 475, 448, 423, 399, 377, 356, 336, 317, 299, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 111, };

// Namespace(bias=-1.0, freq=55930.4, length=64, maxbits=12, upper=53)
// 443.7 8.47550713772 53
const int note_table_tri[64] = {
2138, 2018, 1905, 1798, 1697, 1602, 1512, 1427, 1347, 1272, 1200, 1133, 1069, 1009, 953, 899, 849, 801, 756, 714, 674, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 358, 338, 319, 301, 284, 268, 253, 239, 226, 213, 201, 190, 179, 169, 160, 151, 142, 135, 127, 120, 113, 107, 101, 95, 90, 85, 80, 76, 72, 68, 64, 60, 57, };

#define NOTE_TABLE note_table_49
#define BASS_NOTE 36

byte music_active;
byte cur_duration;

// music data -- see end of file
const byte* music_ptr;

byte is_music_active() {
 return music_active; 
}

byte next_music_byte() {
  return *music_ptr++;
}

void play_music() {
  static byte chs = 0;
  if (music_ptr) {
    // run out duration timer yet?
    while (cur_duration == 0) {
      // fetch next byte in score
      byte note = next_music_byte();
      // is this a note?
      if ((note & 0x80) == 0) {
        // pulse plays higher notes, triangle for lower if it's free
        if (note >= BASS_NOTE || (chs & 4)) {
          int period = NOTE_TABLE[note & 63];
          // see which pulse generator is free
          if (!(chs & 1)) {
            APU_PULSE_DECAY(0, period, DUTY_25, 2, 10);
            chs |= 1;
          } else if (!(chs & 2)) {
            APU_PULSE_DECAY(1, period, DUTY_25, 2, 10);
            chs |= 2;
          }
        } else {
          int period = note_table_tri[note & 63];
          APU_TRIANGLE_LENGTH(period, 15);
          chs |= 4;
        }
      } else {
        // end of score marker
        if (note == 0xff) {
          music_ptr = NULL;
          music_active = 0;
        }
        // set duration until next note
        cur_duration = note & 63;
        // reset channel used mask
        chs = 0;
        
      }
    }
    cur_duration--;
  }
}

void start_music(const byte* music, byte music_offset) {
  music_ptr = music;
  cur_duration = music_offset; // 48 frames for 1st arrow, minus human reaction time (200 ms) = 16 frames = 32
  music_active = 1;
}
/*
void main(void) // Example on how to use music player
{
  apu_init();
  music_ptr = 0;
  while (1) {
    if (!music_ptr) start_music(music1);
    ppu_wait_nmi();
    play_music();
  }
}
*/