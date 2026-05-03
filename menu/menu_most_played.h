/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MENU_MOST_PLAYED_H__
#define __MENU_MOST_PLAYED_H__

#include <boolean.h>
#include <retro_common_api.h>

typedef struct content_playlist playlist_t;
typedef struct most_played_state most_played_state_t;
struct file_list;

RETRO_BEGIN_DECLS

#define MENU_MOST_PLAYED_PLAYLIST_PATH ":most_played:"

int menu_most_played_action_sublabel_spacer(
      struct file_list *list, unsigned type, unsigned i,
      const char *label, const char *path, char *s, size_t len);

most_played_state_t *menu_most_played_build_list(
      const char *directory_playlist,
      const char *directory_runtime_log);
playlist_t *menu_most_played_get_playlist(void);
bool menu_most_played_is_dirty(void);
void menu_most_played_mark_dirty(void);
void menu_most_played_clear_dirty(void);
void menu_most_played_free_state(most_played_state_t *state);
void menu_most_played_free(void);
void menu_most_played_set_state(most_played_state_t *state);

RETRO_END_DECLS

#endif