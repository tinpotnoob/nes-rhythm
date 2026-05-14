#ifndef _MUSIC_PLAYER_H
#define _MUSIC_PLAYER_H

byte is_music_active();
void play_music();
void start_music(const byte* music, byte music_offset);

#endif