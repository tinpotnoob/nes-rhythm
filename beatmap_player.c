/*
Beatmap player, similar to apu code
*/
#include <nes.h>
typedef unsigned char byte;
const byte* beatmap_ptr;
const byte* restmap_ptr;
const byte* beatmap_end;
byte beatmap_active;
byte curr_duration;

byte is_beatmap_active() {
  return beatmap_active;
}
byte next_beatmap_byte() {
  return *beatmap_ptr++;
}
byte next_restmap_byte() {
  return *restmap_ptr++;
}

byte play_beatmap() {
  byte next_arrow = 0;
  if (beatmap_ptr && restmap_ptr) {
    while (curr_duration == 0) {
      if (beatmap_ptr >= beatmap_end) {
        // End the beatmap
        beatmap_ptr = 0;
        restmap_ptr = 0;
        beatmap_active = 0;
        return 0; // Exit early if beatmap is done
      }
      next_arrow = next_beatmap_byte();
      curr_duration = next_restmap_byte(); // Get next duration
    }
    curr_duration--; // Decrease duration for the current arrow
  }
  return next_arrow;
}

void start_beatmap(const byte* beatmap, const byte* restmap, const unsigned int map_size) {
  beatmap_ptr = beatmap;
  restmap_ptr = restmap;
  beatmap_end = beatmap_ptr + map_size;
  curr_duration = 0;
  beatmap_active = 1;
}