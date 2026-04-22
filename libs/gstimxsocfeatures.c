/*
 * Copyright 2026 NXP
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gstimxsocfeatures.h"
#include <stdio.h>
#include <string.h>

#define SOC_GROUP_PREFIX "soc."

/* Internal context */
typedef struct {
  gchar       *soc_id;
  gchar       *chip_code;
  GHashTable  *features;
  GPtrArray   *feature_list;
  GHashTable  *feature_descs;
  GHashTable  *chip_groups;
  gboolean     initialized;
} ImxSocContext;

static ImxSocContext g_ctx = { 0 };
static GMutex g_ctx_mutex;

/* ========== Internal Functions ========== */

static gchar*
read_soc_id_from_system(void)
{
  FILE *fp;
  gchar buf[256];
  gchar *soc_id = NULL;

  fp = fopen("/sys/devices/soc0/soc_id", "r");
  if (!fp)
    return NULL;

  if (fscanf(fp, "%255s", buf) == 1)
    soc_id = g_strdup(buf);

  fclose(fp);
  return soc_id;
}

static gchar*
get_config_path(void)
{
  const gchar *env_path = g_getenv(IMX_SOC_CONFIG_ENV);

  if (env_path && g_file_test(env_path, G_FILE_TEST_EXISTS))
    return g_strdup(env_path);

  if (g_file_test(IMX_SOC_CONFIG_PATH, G_FILE_TEST_EXISTS))
    return g_strdup(IMX_SOC_CONFIG_PATH);

  return NULL;
}

static gchar**
split_value(const gchar *value)
{
  return g_strsplit(value, ";", -1);
}

static void
load_feature_descriptions(GKeyFile *kf)
{
  gchar **keys;
  gsize len, i;

  g_ctx.feature_descs = g_hash_table_new_full(g_str_hash, g_str_equal, 
                                               g_free, g_free);

  keys = g_key_file_get_keys(kf, "feature_descriptions", &len, NULL);
  if (!keys)
    return;

  for (i = 0; i < len; i++) {
    gchar *desc = g_key_file_get_string(kf, "feature_descriptions", keys[i], NULL);
    if (desc)
      g_hash_table_insert(g_ctx.feature_descs, g_strdup(keys[i]), desc);
  }

  g_strfreev(keys);
}

static void
load_chip_groups(GKeyFile *kf)
{
  gchar **keys;
  gsize len, i;

  g_ctx.chip_groups = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, (GDestroyNotify)g_ptr_array_unref);

  keys = g_key_file_get_keys(kf, "chip_groups", &len, NULL);
  if (!keys)
    return;

  for (i = 0; i < len; i++) {
    gchar *value = g_key_file_get_string(kf, "chip_groups", keys[i], NULL);
    if (value) {
      gchar **chips = split_value(value);
      GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);

      for (gint j = 0; chips[j]; j++) {
        g_strstrip(chips[j]);
        if (chips[j][0])
          g_ptr_array_add(arr, g_strdup(chips[j]));
      }

      g_hash_table_insert(g_ctx.chip_groups, g_strdup(keys[i]), arr);
      g_strfreev(chips);
      g_free(value);
    }
  }

  g_strfreev(keys);
}

static gboolean
find_soc_config(GKeyFile *kf, const gchar *soc_id)
{
  gchar **groups;
  gsize num_groups, i;
  gboolean found = FALSE;

  groups = g_key_file_get_groups(kf, &num_groups);
  if (!groups)
    return FALSE;

  for (i = 0; i < num_groups && !found; i++) {
    if (!g_str_has_prefix(groups[i], SOC_GROUP_PREFIX))
      continue;

    gchar *soc_ids_str = g_key_file_get_string(kf, groups[i], "soc_id", NULL);
    if (!soc_ids_str)
      continue;

    gchar **soc_ids = split_value(soc_ids_str);

    for (gint j = 0; soc_ids[j]; j++) {
      g_strstrip(soc_ids[j]);
      if (g_strcmp0(soc_ids[j], soc_id) == 0) {
        g_ctx.chip_code = g_strdup(groups[i] + strlen(SOC_GROUP_PREFIX));
        g_ctx.features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        g_ctx.feature_list = g_ptr_array_new_with_free_func(g_free);

        gchar *features_str = g_key_file_get_string(kf, groups[i], "features", NULL);
        if (features_str) {
          gchar **features = split_value(features_str);

          for (gint k = 0; features[k]; k++) {
            g_strstrip(features[k]);
            if (features[k][0]) {
              g_hash_table_insert(g_ctx.features, g_strdup(features[k]),
                                  GINT_TO_POINTER(TRUE));
              g_ptr_array_add(g_ctx.feature_list, g_strdup(features[k]));
            }
          }

          g_strfreev(features);
          g_free(features_str);
        }

        g_ptr_array_add(g_ctx.feature_list, NULL);
        found = TRUE;
        break;
      }
    }

    g_strfreev(soc_ids);
    g_free(soc_ids_str);
  }

  g_strfreev(groups);
  return found;
}

static void
init_defaults(void)
{
  if (!g_ctx.chip_code)
    g_ctx.chip_code = g_strdup("UNKNOWN");
  if (!g_ctx.features)
    g_ctx.features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  if (!g_ctx.feature_list) {
    g_ctx.feature_list = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(g_ctx.feature_list, NULL);
  }
  if (!g_ctx.feature_descs)
    g_ctx.feature_descs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  if (!g_ctx.chip_groups)
    g_ctx.chip_groups = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, (GDestroyNotify)g_ptr_array_unref);
}

static void
ensure_initialized(void)
{
  g_mutex_lock(&g_ctx_mutex);

  if (g_ctx.initialized) {
    g_mutex_unlock(&g_ctx_mutex);
    return;
  }

  g_ctx.soc_id = read_soc_id_from_system();

  if (g_ctx.soc_id) {
    gchar *config_path = get_config_path();

    if (config_path) {
      GKeyFile *kf = g_key_file_new();

      if (g_key_file_load_from_file(kf, config_path, G_KEY_FILE_NONE, NULL)) {
        load_feature_descriptions(kf);
        load_chip_groups(kf);

        if (!find_soc_config(kf, g_ctx.soc_id))
          init_defaults();
      } else {
        init_defaults();
      }

      g_key_file_free(kf);
      g_free(config_path);
    } else {
      init_defaults();
    }
  } else {
    init_defaults();
  }

  g_ctx.initialized = TRUE;
  g_mutex_unlock(&g_ctx_mutex);
}

/* ========== Public API ========== */

const gchar*
imx_soc_get_soc_id(void)
{
  ensure_initialized();
  return g_ctx.soc_id;
}

const gchar*
imx_soc_get_chip_code(void)
{
  ensure_initialized();
  return g_ctx.chip_code;
}

gboolean
imx_soc_has_feature(const gchar *feature)
{
  if (!feature)
    return FALSE;

  ensure_initialized();
  return g_ctx.features && g_hash_table_contains(g_ctx.features, feature);
}

gboolean
imx_soc_is_chip(const gchar *chip_code)
{
  if (!chip_code)
    return FALSE;

  ensure_initialized();
  return g_strcmp0(g_ctx.chip_code, chip_code) == 0;
}

gboolean
imx_soc_in_group(const gchar *group)
{
  GPtrArray *chips;

  if (!group)
    return FALSE;

  ensure_initialized();

  if (!g_ctx.chip_groups)
    return FALSE;

  chips = g_hash_table_lookup(g_ctx.chip_groups, group);
  if (!chips)
    return FALSE;

  for (guint i = 0; i < chips->len; i++) {
    if (g_strcmp0(g_ctx.chip_code, g_ptr_array_index(chips, i)) == 0)
      return TRUE;
  }

  return FALSE;
}

const gchar* const*
imx_soc_get_features(void)
{
  ensure_initialized();
  return g_ctx.feature_list ? (const gchar* const*)g_ctx.feature_list->pdata : NULL;
}

const gchar*
imx_soc_get_feature_description(const gchar *feature)
{
  if (!feature)
    return NULL;

  ensure_initialized();
  return g_ctx.feature_descs ? g_hash_table_lookup(g_ctx.feature_descs, feature) : NULL;
}