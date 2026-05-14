#ifndef _BEATMAP_PLAYER_
#define _BEATMAP_PLAYER_

byte is_beatmap_active();
byte play_beatmap();
void start_beatmap(const byte* beatmap, const byte* restmap, const unsigned int map_size);

#endif