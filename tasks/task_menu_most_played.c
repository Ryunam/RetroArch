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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

#include "tasks_internal.h"

#include "../menu/menu_driver.h"

typedef struct menu_most_played_init_handle
{
   most_played_state_t *state;
   char *directory_playlist;
   char *directory_runtime_log;
} menu_most_played_init_handle_t;

static void free_menu_most_played_init_handle(
      menu_most_played_init_handle_t *menu_most_played)
{
   if (!menu_most_played)
      return;

   if (menu_most_played->directory_playlist)
   {
      free(menu_most_played->directory_playlist);
      menu_most_played->directory_playlist = NULL;
   }

   if (menu_most_played->directory_runtime_log)
   {
      free(menu_most_played->directory_runtime_log);
      menu_most_played->directory_runtime_log = NULL;
   }

   if (menu_most_played->state)
   {
      menu_most_played_free_state(menu_most_played->state);
      menu_most_played->state = NULL;
   }

   free(menu_most_played);
}

static void cb_task_menu_most_played_init(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   menu_most_played_init_handle_t *menu_most_played = NULL;
   unsigned menu_type                               = 0;
   struct menu_state *menu_st                       = menu_state_get_ptr();

   if (!task)
      return;

   if (!(menu_most_played = (menu_most_played_init_handle_t*)task->state))
      return;

   menu_most_played_set_state(menu_most_played->state);
   menu_most_played->state = NULL;

   menu_entries_get_last_stack(NULL, NULL, &menu_type, NULL, NULL);

   if (menu_type == MENU_SETTING_HORIZONTAL_MENU)
   {
      const menu_ctx_driver_t *driver_ctx = menu_st->driver_ctx;
      if (driver_ctx->list_get_entry)
      {
         size_t selection = driver_ctx->list_get_selection ? driver_ctx->list_get_selection(menu_st->userdata) : 0;
         size_t _len      = driver_ctx->list_get_size      ? driver_ctx->list_get_size(menu_st->userdata, MENU_LIST_TABS) : 0;
         if (selection > 0 && _len > 0)
         {
            struct item_file *item = NULL;
            if ((item = (struct item_file*)driver_ctx->list_get_entry(menu_st->userdata, MENU_LIST_HORIZONTAL,
                        (unsigned)(selection - (_len +1)))))
               menu_type = item->type;
         }
      }
   }

   if (menu_type == MENU_MOST_PLAYED_TAB)
      menu_st->flags            |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                 |  MENU_ST_FLAG_PREVENT_POPULATE;
}

static void task_menu_most_played_init_free(retro_task_t *task)
{
   menu_most_played_init_handle_t *menu_most_played = NULL;

   if (!task)
      return;

   menu_most_played = (menu_most_played_init_handle_t*)task->state;

   free_menu_most_played_init_handle(menu_most_played);
}

static void task_menu_most_played_init_handler(retro_task_t *task)
{
   if (task)
   {
      menu_most_played_init_handle_t *menu_most_played = NULL;
      if ((menu_most_played = (menu_most_played_init_handle_t*)task->state))
      {
         uint8_t flg = task_get_flags(task);

         if (!((flg & RETRO_TASK_FLG_CANCELLED) > 0))
         {
            menu_most_played->state = menu_most_played_build_list(
                  menu_most_played->directory_playlist,
                  menu_most_played->directory_runtime_log);

            task_set_progress(task, 100);
         }
      }

      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }
}

static bool task_menu_most_played_init_finder(retro_task_t *task, void *user_data)
{
   return (task && task->handler == task_menu_most_played_init_handler);
}

bool task_push_menu_most_played_init(const char *directory_playlist,
      const char *directory_runtime_log)
{
   task_finder_data_t find_data;
   retro_task_t *task                             = NULL;
   menu_most_played_init_handle_t *menu_most_played = NULL;

   if (   string_is_empty(directory_playlist)
       || string_is_empty(directory_runtime_log))
      goto error;

   task             = task_init();
   menu_most_played = (menu_most_played_init_handle_t*)calloc(1,
         sizeof(menu_most_played_init_handle_t));

   if (!task || !menu_most_played)
      goto error;

   find_data.func     = task_menu_most_played_init_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      goto error;

   menu_most_played->state                 = NULL;
   menu_most_played->directory_playlist    = strdup(directory_playlist);
   menu_most_played->directory_runtime_log = strdup(directory_runtime_log);

   task->handler  = task_menu_most_played_init_handler;
   task->state    = menu_most_played;
   task->title    = NULL;
   task->progress = 0;
   task->callback = cb_task_menu_most_played_init;
   task->cleanup  = task_menu_most_played_init_free;
   task->flags   |= RETRO_TASK_FLG_MUTE;

   task_queue_push(task);

   return true;

error:

   if (task)
      free(task);

   free_menu_most_played_init_handle(menu_most_played);

   return false;
}

bool menu_most_played_init_in_progress(void *data)
{
   task_finder_data_t find_data;

   find_data.func     = task_menu_most_played_init_finder;
   find_data.userdata = NULL;

   if (task_queue_find(&find_data))
      return true;

   return false;
}

void menu_most_played_wait_for_init_task(void)
{
   task_queue_wait(menu_most_played_init_in_progress, NULL);
}
