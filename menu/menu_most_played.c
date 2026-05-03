/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>

#include <array/rbuf.h>
#include <array/rhmap.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "menu_most_played.h"
#include "menu_driver.h"
#include "../configuration.h"
#include "../config.def.h"
#include "../file_path_special.h"
#include "../msg_hash.h"
#include "../playlist.h"

typedef struct
{
   const struct playlist_entry *entry;
} most_played_source_t;

typedef struct
{
   char key[NAME_MAX_LENGTH];
   uint32_t runtime_seconds;
} most_played_runtime_t;

typedef struct
{
   const struct playlist_entry *entry;
   unsigned rank;
} most_played_selected_entry_t;

struct most_played_state
{
   playlist_t *playlist;
};

static most_played_state_t *most_played_state = NULL;
static bool most_played_is_dirty             = false;

int menu_most_played_action_sublabel_spacer(
      struct file_list *list, unsigned type, unsigned i,
      const char *label, const char *path, char *s, size_t len)
{
   const char *sublabel =
         msg_hash_to_str(MENU_ENUM_SUBLABEL_CONTENT_MOST_PLAYED_SIZE);

   if (len > 0)
      s[0] = '\0';

   if (!string_is_empty(sublabel))
      strlcpy(s, sublabel, len);

#ifdef HAVE_OZONE
   const char *menu_driver = menu_driver_ident();
   /* Keep the regular sublabel text, but add an extra
    * spacer line for Ozone so content rows don't crowd
    * the setting row. */
   if (memcmp(menu_driver, "ozone", STRLEN_CONST("ozone") + 1) == 0)
   {
      size_t _len = strlen(s);

      if (_len + 2 < len)
      {
         s[_len]     = '\n';
         s[_len + 1] = ' ';
         s[_len + 2] = '\0';
      }
   }
#endif
   return 1; /* 1 means it'll never change and can be cached */
}

static bool menu_most_played_get_key(
      const char *path, char *key, size_t len)
{
   const char *base = NULL;

   if (string_is_empty(path) || !key || (len < 2))
      return false;

   base = path_basename_nocompression(path);

   if (string_is_empty(base))
      return false;

   strlcpy(key, base, len);
   path_remove_extension(key);
   string_to_lower(key);

   return !string_is_empty(key);
}

static bool menu_most_played_read_runtime_seconds(
      const char *path, uint32_t *runtime_seconds)
{
   char *runtime_pos = NULL;
   char *runtime_val = NULL;
   char *runtime_end = NULL;
   char runtime_str[64];
   void *buf         = NULL;
   int64_t len       = 0;
   unsigned hours    = 0;
   unsigned minutes  = 0;
   unsigned seconds  = 0;

   if (!runtime_seconds)
      return false;

   *runtime_seconds = 0;

   if (filestream_read_file(path, &buf, &len) != 1 || !buf || (len <= 0))
      return false;

   runtime_pos = strstr((char*)buf, "\"runtime\"");
   if (!runtime_pos)
      goto end;

   runtime_pos += STRLEN_CONST("\"runtime\"");

   while (*runtime_pos == ' ' || *runtime_pos == '\t' || *runtime_pos == ':')
      runtime_pos++;

   if (*runtime_pos != '\"')
      goto end;

   runtime_val = runtime_pos + 1;
   runtime_end = strchr(runtime_val, '\"');

   if (!runtime_end)
      goto end;

   if ((size_t)(runtime_end - runtime_val) >= sizeof(runtime_str))
      goto end;

   memcpy(runtime_str, runtime_val, (size_t)(runtime_end - runtime_val));
   runtime_str[runtime_end - runtime_val] = '\0';

   if (sscanf(runtime_str, "%u:%u:%u", &hours, &minutes, &seconds) == 3)
      *runtime_seconds = (hours * 3600) + (minutes * 60) + seconds;
   else if (sscanf(runtime_str, "%u:%u", &minutes, &seconds) == 2)
      *runtime_seconds = (minutes * 60) + seconds;
   else if (sscanf(runtime_str, "%u", &seconds) == 1)
      *runtime_seconds = seconds;

end:
   if (buf)
      free(buf);

   return (*runtime_seconds > 0);
}

static int menu_most_played_qsort_runtime_desc(
      const void *a_, const void *b_)
{
   const most_played_runtime_t *a = (const most_played_runtime_t*)a_;
   const most_played_runtime_t *b = (const most_played_runtime_t*)b_;

   if (a->runtime_seconds > b->runtime_seconds)
      return -1;
   if (a->runtime_seconds < b->runtime_seconds)
      return 1;

   return strcasecmp(a->key, b->key);
}

most_played_state_t *menu_most_played_build_list(
      const char *directory_playlist,
      const char *directory_runtime_log)
{
   settings_t *settings                          = config_get_ptr();
   size_t i;
   playlist_t **source_playlists                  = NULL;
   most_played_source_t *source_map               = NULL;
   most_played_runtime_t *runtime_list            = NULL;
   most_played_selected_entry_t *selected_entries = NULL;
   size_t *runtime_index_map                      = NULL;
   struct string_list runtime_files;
   struct string_list playlist_files;
   playlist_config_t playlist_config;
   most_played_state_t *state                     = NULL;
   unsigned content_most_played_size              =
         (settings && (settings->uints.content_most_played_size > 0))
         ? settings->uints.content_most_played_size
         : DEFAULT_CONTENT_MOST_PLAYED_SIZE;
   unsigned selected_count                        = 0;
   bool runtime_files_inited                      = false;
   bool playlist_files_inited                     = false;

   if (content_most_played_size > 9999)
      content_most_played_size                    = 9999;

   if (   string_is_empty(directory_playlist)
       || string_is_empty(directory_runtime_log))
      return NULL;

   state = (most_played_state_t*)calloc(1, sizeof(*state));
   if (!state)
      goto end;

   playlist_config_set_path(&playlist_config, MENU_MOST_PLAYED_PLAYLIST_PATH);
   playlist_config.capacity            = content_most_played_size;
   playlist_config.old_format          = false;
   playlist_config.compress            = false;
   playlist_config.fuzzy_archive_match = false;
   playlist_config_set_base_content_directory(&playlist_config, NULL);

   state->playlist = playlist_init(&playlist_config);
   if (!state->playlist)
      goto end;

   playlist_clear(state->playlist);

   if (!dir_list_initialize(&runtime_files,
            directory_runtime_log,
            "lrtl", false, false, false, false))
      goto end;

   runtime_files_inited = true;

   for (i = 0; i < runtime_files.size; i++)
   {
      uint32_t runtime_seconds = 0;
      size_t runtime_idx       = 0;
      char runtime_key[NAME_MAX_LENGTH];
      const char *runtime_path = runtime_files.elems[i].data;

      if (string_is_empty(runtime_path))
         continue;

      if (!menu_most_played_get_key(runtime_path,
               runtime_key, sizeof(runtime_key)))
         continue;

      if (!menu_most_played_read_runtime_seconds(
               runtime_path, &runtime_seconds))
         continue;

      runtime_idx = RHMAP_GET_STR(runtime_index_map, runtime_key);

      if (runtime_idx > 0)
      {
         if (runtime_seconds > runtime_list[runtime_idx - 1].runtime_seconds)
            runtime_list[runtime_idx - 1].runtime_seconds = runtime_seconds;
         continue;
      }

      {
         most_played_runtime_t runtime_item;
         runtime_item.key[0]         = '\0';
         runtime_item.runtime_seconds = runtime_seconds;
         strlcpy(runtime_item.key, runtime_key, sizeof(runtime_item.key));
         RBUF_PUSH(runtime_list, runtime_item);
      }

      RHMAP_SET_STR(runtime_index_map, runtime_key, RBUF_LEN(runtime_list));
   }

   if (!RBUF_LEN(runtime_list))
      goto end;

   if (!dir_list_initialize(&playlist_files,
         directory_playlist,
            "lpl", false, false, false, false))
      goto end;

   playlist_files_inited = true;

   for (i = 0; i < playlist_files.size; i++)
   {
      playlist_t *playlist          = NULL;
      size_t j                      = 0;
      const char *playlist_path     = playlist_files.elems[i].data;
      const char *playlist_basename = path_basename(playlist_path);

      if (string_is_empty(playlist_path) || string_is_empty(playlist_basename))
         continue;

      if (   string_is_equal(playlist_basename, FILE_PATH_CONTENT_FAVORITES)
           || string_is_equal(playlist_basename, FILE_PATH_CONTENT_HISTORY))
         continue;

      playlist_config_set_path(&playlist_config, playlist_path);
      playlist_config.capacity            = COLLECTION_SIZE;
      playlist_config.old_format          = false;
      playlist_config.compress            = false;
      playlist_config.fuzzy_archive_match = false;
      playlist_config_set_base_content_directory(&playlist_config, NULL);

      playlist = playlist_init(&playlist_config);

      if (!playlist)
         continue;

      RBUF_PUSH(source_playlists, playlist);

      for (j = 0; j < playlist_size(playlist); j++)
      {
         most_played_source_t source_item;
         char entry_key[NAME_MAX_LENGTH];
         const struct playlist_entry *entry = NULL;

         playlist_get_index(playlist, j, &entry);

         if (!entry || string_is_empty(entry->path) || string_is_empty(entry->label))
            continue;

         if (!menu_most_played_get_key(
                  entry->path, entry_key, sizeof(entry_key)))
            continue;

         if (RHMAP_GET_STR(runtime_index_map, entry_key) == 0)
            continue;

         if (RHMAP_HAS_STR(source_map, entry_key))
            continue;

         source_item.entry = entry;
         RHMAP_SET_STR(source_map, entry_key, source_item);
      }
   }

   if (!RHMAP_LEN(source_map))
      goto end;

   qsort(runtime_list,
         RBUF_LEN(runtime_list),
         sizeof(*runtime_list),
         menu_most_played_qsort_runtime_desc);

   for (i = 0; i < RBUF_LEN(runtime_list); i++)
   {
      most_played_source_t source_item = RHMAP_GET_STR(source_map, runtime_list[i].key);

      if (!source_item.entry)
         continue;

      if (string_is_empty(source_item.entry->core_path))
         continue;

      {
         most_played_selected_entry_t selected;
         selected.rank  = selected_count + 1;
         selected.entry = source_item.entry;
         RBUF_PUSH(selected_entries, selected);
      }

      selected_count++;

      if (selected_count >= content_most_played_size)
         break;
   }

   for (i = selected_count; i > 0; i--)
   {
      const most_played_selected_entry_t *selected = &selected_entries[i - 1];
      struct playlist_entry out_entry              = *(selected->entry);
      char numbered_label[PATH_MAX_LENGTH + NAME_MAX_LENGTH];

      snprintf(numbered_label, sizeof(numbered_label),
            "%03u - %s",
            selected->rank,
            string_is_empty(selected->entry->label) ? "" : selected->entry->label);

      out_entry.label = numbered_label;
      playlist_push(state->playlist, &out_entry);
   }

end:
   if (playlist_files_inited)
      dir_list_deinitialize(&playlist_files);
   if (runtime_files_inited)
      dir_list_deinitialize(&runtime_files);

   for (i = 0; i < RBUF_LEN(source_playlists); i++)
      playlist_free(source_playlists[i]);

   RHMAP_FREE(source_map);
   RHMAP_FREE(runtime_index_map);
   RBUF_FREE(runtime_list);
   RBUF_FREE(selected_entries);
   RBUF_FREE(source_playlists);

   return state;
}

playlist_t *menu_most_played_get_playlist(void)
{
   if (!most_played_state)
      return NULL;

   return most_played_state->playlist;
}

bool menu_most_played_is_dirty(void)
{
   return most_played_is_dirty;
}

void menu_most_played_mark_dirty(void)
{
   most_played_is_dirty = true;
}

void menu_most_played_clear_dirty(void)
{
   most_played_is_dirty = false;
}

void menu_most_played_free_state(most_played_state_t *state)
{
   if (!state)
      return;

   if (state->playlist)
   {
      if (playlist_get_cached() == state->playlist)
         playlist_set_cached_external(NULL);

      playlist_free(state->playlist);
      state->playlist = NULL;
   }

   free(state);
}

void menu_most_played_free(void)
{
   if (!most_played_state)
      return;

   menu_most_played_free_state(most_played_state);
   most_played_state = NULL;
}

void menu_most_played_set_state(most_played_state_t *state)
{
   if (most_played_state)
      menu_most_played_free();

   most_played_state = state;
}
