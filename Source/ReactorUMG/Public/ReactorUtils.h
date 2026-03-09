/**
 * @file ReactorUtils.h
 * @brief Filesystem, path-resolution, and project-layout utilities
 *        used throughout the ReactorUMG plugin.
 *
 * All functions are static and stateless. Heavy filesystem operations
 * (recursive copy/delete) use the platform abstraction layer so they
 * work correctly across Win64, Mac, Linux, Android, and iOS.
 */

#pragma once

#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "CoreMinimal.h"

/**
 * @class FReactorUtils
 * @brief Collection of static utility helpers for file I/O,
 *        path resolution, and TypeScript project discovery.
 */
class REACTORUMG_API FReactorUtils
{
public:

	// ---------------------------------------------------------------
	//  Filesystem operations
	// ---------------------------------------------------------------

	/**
	 * @brief Recursively copy all files and subdirectories from
	 *        @p SrcDir into @p DestDir.
	 *
	 * Files whose relative paths appear in @p SkipExistFiles are
	 * skipped if they already exist in the destination, preventing
	 * accidental overwrite of user-modified files.
	 *
	 * @param SrcDir          Source directory to copy from.
	 * @param DestDir         Destination directory to copy into.
	 * @param SkipExistFiles  Relative paths (may omit extension) to skip.
	 * @return                True if the destination tree was created.
	 */
	static bool CopyDirectoryRecursive(const FString& SrcDir, const FString& DestDir, const TArray<FString>& SkipExistFiles = {});

	/**
	 * @brief Copy a single file, creating intermediate directories as needed.
	 * @param SrcFile   Absolute path to the source file.
	 * @param DestFile  Absolute path to the destination file.
	 */
	static void CopyFile(const FString& SrcFile, const FString& DestFile);

	/**
	 * @brief Delete a file if it exists.
	 * @param FilePath  Absolute path to the file to delete.
	 */
	static void DeleteFile(const FString& FilePath);

	/**
	 * @brief Get the absolute path to the ReactorUMG plugin's Content directory.
	 * @return Full filesystem path.
	 */
	static FString GetPluginContentDir();

	/**
	 * @brief Get the absolute path to the ReactorUMG plugin's root directory.
	 * @return Full filesystem path.
	 */
	static FString GetPluginDir();

	/**
	 * @brief Recursively delete a directory and all its contents.
	 * @param DirPath  Absolute path to the directory to remove.
	 * @return         True if successful (or directory did not exist).
	 */
	static bool DeleteDirectoryRecursive(const FString& DirPath);

	/**
	 * @brief Create a directory tree, including all intermediate directories.
	 * @param DirPath  Absolute path to the directory to create.
	 * @return         True if the directory now exists.
	 */
	static bool CreateDirectoryRecursive(const FString& DirPath);

	/**
	 * @brief Thin wrapper around IPlatformFile::CopyDirectoryTree.
	 * @param Src       Source directory.
	 * @param Dest      Destination directory.
	 * @param Overrided Whether to overwrite existing files.
	 * @return          True on success.
	 */
	FORCEINLINE static bool CopyDirectoryTree(const FString& Src, const FString& Dest, bool Overrided)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		return PlatformFile.CopyDirectoryTree(*Dest, *Src, Overrided);
	}

	// ---------------------------------------------------------------
	//  TypeScript project layout helpers
	// ---------------------------------------------------------------

	/**
	 * @brief Resolve the TypeScript project home directory from plugin settings.
	 *
	 * Falls back to "<ProjectDir>/TypeScript" if the setting is empty.
	 *
	 * @return Absolute path to the TS project root.
	 */
	static FString GetTypeScriptHomeDir();

	/**
	 * @brief Check if @p CheckName matches any entry in the skip-list.
	 * @param SkipExistFiles  Array of names/paths to compare against.
	 * @param CheckName       The relative path to check.
	 * @return                True if a match was found.
	 */
	static bool CheckNameExistInArray(const TArray<FString>& SkipExistFiles, const FString& CheckName);

	/**
	 * @brief Read an entire file into a string.
	 * @param FilePath    Absolute path to the file.
	 * @param OutContent  [out] The file's text content.
	 * @return            True if the read succeeded.
	 */
	static bool ReadFileContent(const FString& FilePath, FString& OutContent);

	/**
	 * @brief Resolve a relative path using tsconfig.json path aliases.
	 *
	 * Walks up from @p DirName to find tsconfig.json, then uses its
	 * compilerOptions.paths mappings to expand the relative import.
	 *
	 * @param RelativePath  The import path to resolve.
	 * @param DirName       Starting directory for the tsconfig search.
	 * @return              Fully resolved absolute path.
	 */
	static FString ConvertRelativePathToFullUsingTSConfig(const FString& RelativePath, const FString& DirName);

	/**
	 * @brief Read compilerOptions.outDir from tsconfig.json and return
	 *        its absolute path.
	 * @param ProjectDir  Directory containing tsconfig.json.
	 * @return            Absolute path to the output directory.
	 */
	static FString GetTSCBuildOutDirFromTSConfig(const FString& ProjectDir);

	/**
	 * @brief Get or create the gameplay TypeScript home directory.
	 *
	 * Path pattern: <TSHomeDir>/src/<ProjectName>/
	 *
	 * @return Absolute path to the gameplay TS directory.
	 */
	static FString GetGamePlayTSHomeDir();

	/**
	 * @brief Get the absolute path to the gameplay entry point JS file.
	 * @return Path to start_game.js within the build output.
	 */
	static FString GetGamePlayStartPoint();

	/**
	 * @brief Check whether any Play-In-Editor session is currently active.
	 * @return True if at least one PIE world context exists.
	 */
	static bool IsAnyPIERunning();
};
