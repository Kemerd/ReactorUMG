/**
 * @file ReactorUMGVersion.h
 * @brief Centralized Unreal Engine version compatibility macros for ReactorUMG.
 *
 * All engine-version branching should use the macros defined here rather
 * than raw ENGINE_MAJOR_VERSION / ENGINE_MINOR_VERSION checks scattered
 * across individual source files. This guarantees forward-compatibility
 * with future engine releases (5.5, 5.6, 6.0, ...) by using >= semantics
 * instead of == comparisons.
 *
 * Usage:
 * @code
 *   #include "ReactorUMGVersion.h"
 *
 *   #if REACTORUMG_UE_AT_LEAST(5, 4)
 *       // 5.4+ code path
 *   #else
 *       // pre-5.4 fallback
 *   #endif
 * @endcode
 */

#pragma once

#include "Runtime/Launch/Resources/Version.h"

/* -----------------------------------------------------------------------
 *  Core version arithmetic
 *
 *  Encodes the engine version as a single comparable integer:
 *    UE 5.4  -> 504
 *    UE 5.5  -> 505
 *    UE 6.0  -> 600
 * ----------------------------------------------------------------------- */

/** @brief Combined engine version as a single integer (Major * 100 + Minor). */
#define REACTORUMG_UE_VERSION \
	(ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

/**
 * @brief Evaluates to true if the engine version is >= Major.Minor.
 * @param Major  Engine major version (e.g. 5).
 * @param Minor  Engine minor version (e.g. 4).
 */
#define REACTORUMG_UE_AT_LEAST(Major, Minor) \
	(REACTORUMG_UE_VERSION >= ((Major) * 100 + (Minor)))

/* -----------------------------------------------------------------------
 *  Feature-flag macros
 *
 *  Each flag maps a specific API change to the engine version that
 *  introduced it. Add new flags here as future engine releases
 *  deprecate or rename APIs.
 * ----------------------------------------------------------------------- */

/**
 * @brief UWidget::SynchronizeProperties() availability.
 *
 * This virtual was exposed on UWidget starting in UE 5.3.
 * On earlier versions, only UPanelSlot::SynchronizeProperties() exists.
 */
#define REACTORUMG_HAS_WIDGET_SYNC_PROPERTIES \
	REACTORUMG_UE_AT_LEAST(5, 3)

/**
 * @brief FEditorDelegates::OnPreForceDeleteObjects availability.
 *
 * Replaced FEditorDelegates::OnAssetsPreDelete starting in UE 5.4.
 */
#define REACTORUMG_HAS_ON_PRE_FORCE_DELETE \
	REACTORUMG_UE_AT_LEAST(5, 4)

/**
 * @brief UAssetEditorSubsystem::OnAssetClosedInEditor availability.
 *
 * Replaced OnAssetEditorRequestClose starting in UE 5.4.
 * The new delegate signature includes an IAssetEditorInstance* parameter.
 */
#define REACTORUMG_HAS_ON_ASSET_CLOSED_IN_EDITOR \
	REACTORUMG_UE_AT_LEAST(5, 4)

/**
 * @brief Legacy FAssetTypeActions_Base::GetActions override.
 *
 * Only available on UE 5.0 and 5.1. Removed in UE 5.2 as part of
 * the migration towards UAssetDefinitionDefault.
 */
#define REACTORUMG_HAS_LEGACY_ASSET_GET_ACTIONS \
	(!REACTORUMG_UE_AT_LEAST(5, 2))

/**
 * @brief MulticastInlineDelegateProperty / MulticastSparseDelegateProperty.
 *
 * These property types were introduced in UE 4.23 and are present in
 * all UE5 versions. The combined version check handles both UE4.23+
 * and the entire UE5+ range correctly.
 */
#define REACTORUMG_HAS_MULTICAST_INLINE_DELEGATE \
	REACTORUMG_UE_AT_LEAST(4, 23)
