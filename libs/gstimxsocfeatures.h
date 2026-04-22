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

#ifndef __GST_IMX_SOC_FEATURES_H__
#define __GST_IMX_SOC_FEATURES_H__

#include <glib.h>

G_BEGIN_DECLS

#define IMX_SOC_CONFIG_ENV  "IMX_SOC_CONFIG"
#define IMX_SOC_CONFIG_PATH "/usr/share/imx_soc_features.ini"

/**
 * imx_soc_get_soc_id:
 *
 * Get the SoC ID string read from system.
 *
 * Returns: (transfer none): SoC ID (e.g., "i.MX8MP") or NULL
 */
const gchar* imx_soc_get_soc_id(void);

/**
 * imx_soc_get_chip_code:
 *
 * Get the chip code from configuration.
 *
 * Returns: (transfer none): Chip code (e.g., "MX8MP") or "UNKNOWN"
 */
const gchar* imx_soc_get_chip_code(void);

/**
 * imx_soc_has_feature:
 * @feature: feature name to check
 *
 * Check if a feature is supported on current SoC.
 *
 * Returns: TRUE if supported
 */
gboolean imx_soc_has_feature(const gchar *feature);

/**
 * imx_soc_is_chip:
 * @chip_code: chip code to match
 *
 * Check if running on a specific chip.
 *
 * Returns: TRUE if matched
 */
gboolean imx_soc_is_chip(const gchar *chip_code);

/**
 * imx_soc_in_group:
 * @group: group name to check
 *
 * Check if current chip belongs to a group.
 *
 * Returns: TRUE if in group
 */
gboolean imx_soc_in_group(const gchar *group);

/**
 * imx_soc_get_features:
 *
 * Get list of all supported features.
 *
 * Returns: (transfer none): NULL-terminated array of feature names
 */
const gchar* const* imx_soc_get_features(void);

/**
 * imx_soc_get_feature_description:
 * @feature: feature name
 *
 * Get description of a feature.
 *
 * Returns: (transfer none): description string or NULL
 */
const gchar* imx_soc_get_feature_description(const gchar *feature);

G_END_DECLS

#endif /* __GST_IMX_SOC_FEATURES_H__ */