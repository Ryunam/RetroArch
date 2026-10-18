/* Single-source definitions: run-ahead audio offset.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_RUNAHEAD; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_RUNAHEAD) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT(run_ahead_audio_offset, RUN_AHEAD_AUDIO_OFFSET,
      "run_ahead_audio_offset",
      RUNAHEAD_AUDIO_OFFSET_OFF, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MAX_RUNAHEAD_FRAMES, 1, 0,
      setting_action_ok_uint, setting_get_string_representation_uint_run_ahead_audio_offset,
      "Run-Ahead Audio Offset",
      "Offset audio playback behind the run-ahead video by the defined number of frames, up to the chosen value for Run-Ahead Frames or Preemptive Frames. The offset stays the same when that number changes, unless it drops below the offset, which is then lowered to match. Prevents pops in games whose sound reacts to input before their picture does, where run-ahead would otherwise cut into their sound effects. Applies to Single Instance and Preemptive Frames modes only; Second Instance mode is unaffected, as its audio does not run ahead.")
#endif
