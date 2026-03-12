#!/usr/bin/env python3
"""
ReactorUMG Windows Setup Script

Symlink-safe replacement for setup_win.bat. Correctly resolves paths
even when the plugin directory is symlinked into a UE project's
Plugins/ folder.

Usage:
    python setup_win.py
    python setup_win.py --project-root "C:/MyProject"

The script will:
  1. Locate the UE project root (searches for .uproject)
  2. Download V8 engine binaries if missing
  3. Copy the TypeScript template into <ProjectRoot>/TypeScript/
  4. Copy system JS files into <ProjectRoot>/Content/JavaScript/
  5. Run yarn build
"""

import argparse
import glob
import io
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

# ---------------------------------------------------------------------------
#  Configuration (override via environment variables)
# ---------------------------------------------------------------------------

V8_VERSION      = os.environ.get("REACTORUMG_V8_VERSION",      "v8_bin_9.4.146.24")
V8_VERSION_NAME = os.environ.get("REACTORUMG_V8_VERSION_NAME", "v8_9.4.146.24")
V8_URL          = os.environ.get("REACTORUMG_V8_URL",
    f"https://github.com/puerts/backend-v8/releases/download/"
    f"V8_9.4.146.24__241009/{V8_VERSION}.tgz")

NODE_VERSION = os.environ.get("REACTORUMG_NODE_VERSION", "18.20.4")


# ---------------------------------------------------------------------------
#  Helpers
# ---------------------------------------------------------------------------

def log(msg: str) -> None:
    print(f"  {msg}")


def log_header(msg: str) -> None:
    print(f"\n{'=' * 60}")
    print(f"  {msg}")
    print(f"{'=' * 60}")


def run(cmd: str, cwd: str | None = None, check: bool = True) -> int:
    """Run a shell command, streaming output. Returns exit code."""
    result = subprocess.run(cmd, shell=True, cwd=cwd)
    if check and result.returncode != 0:
        print(f"\n  ERROR: Command failed (exit {result.returncode}): {cmd}")
        sys.exit(result.returncode)
    return result.returncode


def which(name: str) -> str | None:
    """Cross-platform shutil.which wrapper."""
    return shutil.which(name)


def find_uproject(start_dir: str, max_depth: int = 6) -> str | None:
    """Walk up from start_dir looking for a .uproject file."""
    current = os.path.abspath(start_dir)
    for _ in range(max_depth):
        matches = glob.glob(os.path.join(current, "*.uproject"))
        if matches:
            return current
        parent = os.path.dirname(current)
        if parent == current:
            break
        current = parent
    return None


# ---------------------------------------------------------------------------
#  Path resolution (the whole reason this script exists)
# ---------------------------------------------------------------------------

def resolve_paths(args: argparse.Namespace) -> dict:
    """
    Resolve PLUGIN_DIR and PROJECT_ROOT correctly even through symlinks.

    The bat script uses %~dp0 which resolves symlinks, losing the
    actual project location. We handle it differently:

      PLUGIN_DIR  = real path of the plugin (for V8/source files)
      PROJECT_ROOT = the UE project that contains the plugin

    To find PROJECT_ROOT when symlinked, we walk the symlink chain.
    If Tools/setup_win.py lives at:
        <real_plugin>/Tools/setup_win.py
    And there's a symlink:
        <project>/Plugins/ReactorUMG -> <real_plugin>
    We need to find <project>.

    Strategy:
      1. If --project-root is given, use it
      2. Search CWD upward for .uproject
      3. Check if there's a symlink pointing to our plugin in some
         Plugins/ dir by scanning common locations
    """
    # The real plugin directory (resolves all symlinks)
    script_real = os.path.realpath(__file__)
    plugin_dir = os.path.dirname(os.path.dirname(script_real))

    # -- Strategy 1: explicit argument --
    if args.project_root:
        project_root = os.path.abspath(args.project_root)
        if not os.path.isdir(project_root):
            print(f"  ERROR: --project-root '{project_root}' does not exist.")
            sys.exit(1)
        return {"plugin_dir": plugin_dir, "project_root": project_root}

    # -- Strategy 2: search from CWD --
    cwd_project = find_uproject(os.getcwd())
    if cwd_project:
        log(f"Found project root from CWD: {cwd_project}")
        return {"plugin_dir": plugin_dir, "project_root": cwd_project}

    # -- Strategy 3: find symlink pointing to us --
    # The script's __file__ path before realpath may contain the
    # symlink chain if Python preserved it (CPython does on Windows)
    script_apparent = os.path.abspath(__file__)
    apparent_plugin = os.path.dirname(os.path.dirname(script_apparent))

    if apparent_plugin != plugin_dir:
        # script_apparent is through the symlink
        apparent_project = find_uproject(apparent_plugin)
        if apparent_project:
            log(f"Found project root via symlink: {apparent_project}")
            return {"plugin_dir": plugin_dir, "project_root": apparent_project}

    # -- Strategy 4: assume Plugins/ReactorUMG layout from real path --
    # plugin_dir/../.. might be the project root
    guess = os.path.dirname(os.path.dirname(plugin_dir))
    guess_project = find_uproject(guess, max_depth=1)
    if guess_project:
        log(f"Found project root from plugin parent: {guess_project}")
        return {"plugin_dir": plugin_dir, "project_root": guess_project}

    # -- Give up, ask the user --
    print("\n  Could not auto-detect the UE project root.")
    print("  Please re-run with --project-root:")
    print(f'    python "{script_real}" --project-root "C:/path/to/your/project"')
    sys.exit(1)


# ---------------------------------------------------------------------------
#  Setup steps
# ---------------------------------------------------------------------------

def ensure_tools() -> dict:
    """Make sure node, npm, and yarn are available. Returns paths."""
    tools = {}

    # Node
    node = which("node")
    if not node:
        print("  ERROR: Node.js not found. Install Node.js >= 18 and re-run.")
        print("         https://nodejs.org/")
        sys.exit(1)
    tools["node"] = node
    log(f"Node.js: {node}")

    # npm
    npm = which("npm")
    if not npm:
        print("  ERROR: npm not found.")
        sys.exit(1)
    tools["npm"] = npm

    # yarn (install locally if missing)
    yarn = which("yarn")
    if not yarn:
        log("Yarn not found, installing via npm...")
        run("npm install -g yarn")
        yarn = which("yarn")
        if not yarn:
            print("  ERROR: Yarn still not available after install.")
            sys.exit(1)
    tools["yarn"] = yarn
    log(f"Yarn:    {yarn}")

    return tools


def download_v8(plugin_dir: str) -> None:
    """Download and extract V8 binaries if they don't exist."""
    third_party = os.path.join(plugin_dir, "Source", "Puerts", "ThirdParty")
    v8_target = os.path.join(third_party, V8_VERSION_NAME)

    if os.path.isdir(v8_target):
        log(f"V8 already present at {v8_target} -- skipping.")
        return

    os.makedirs(third_party, exist_ok=True)

    log(f"Downloading V8 from:\n    {V8_URL}")
    archive_path = os.path.join(tempfile.gettempdir(), f"{V8_VERSION}.tgz")

    try:
        urllib.request.urlretrieve(V8_URL, archive_path)
    except Exception as e:
        print(f"  ERROR: Failed to download V8: {e}")
        print(f"  You can override the URL via REACTORUMG_V8_URL env var.")
        sys.exit(1)

    log(f"Extracting V8 to {third_party}")
    try:
        with tarfile.open(archive_path, "r:gz") as tar:
            tar.extractall(path=third_party)
    except Exception as e:
        print(f"  ERROR: Failed to extract V8: {e}")
        sys.exit(1)

    # Cleanup archive
    try:
        os.remove(archive_path)
    except OSError:
        pass

    if os.path.isdir(v8_target):
        log("V8 extraction successful.")
    else:
        print(f"  WARNING: Expected V8 at {v8_target} but it wasn't found.")
        print(f"  The archive may have extracted with a different folder name.")
        # List what was extracted so the user can see
        contents = os.listdir(third_party)
        print(f"  ThirdParty contents: {contents}")


def copy_ts_template(plugin_dir: str, project_root: str) -> str:
    """Copy the TypeScript project template if it doesn't exist yet."""
    template_dir = os.path.join(plugin_dir, "Scripts", "Project")
    ts_dir = os.path.join(project_root, "TypeScript")

    if os.path.isdir(ts_dir):
        log(f"TypeScript workspace already exists at {ts_dir}")
        return ts_dir

    if not os.path.isdir(template_dir):
        print(f"  ERROR: Template not found at {template_dir}")
        sys.exit(1)

    log(f"Copying TypeScript template to {ts_dir}")
    shutil.copytree(template_dir, ts_dir)
    return ts_dir


def copy_system_js(plugin_dir: str, project_root: str) -> None:
    """Copy predefined system JS files into Content/JavaScript/."""
    src = os.path.join(plugin_dir, "Scripts", "System", "JavaScript")
    dst = os.path.join(project_root, "Content", "JavaScript")

    if not os.path.isdir(src):
        log("No system JS files to copy (directory missing).")
        return

    os.makedirs(dst, exist_ok=True)

    # Copy tree without overwriting existing files
    for root, dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        dst_root = os.path.join(dst, rel)
        os.makedirs(dst_root, exist_ok=True)
        for f in files:
            dst_file = os.path.join(dst_root, f)
            if not os.path.exists(dst_file):
                shutil.copy2(os.path.join(root, f), dst_file)

    log(f"System JS files copied to {dst}")


def run_yarn_build(ts_dir: str) -> None:
    """Install dependencies and build the TypeScript project."""
    log(f"Running yarn build in {ts_dir}")
    run("yarn build", cwd=ts_dir)


# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="ReactorUMG setup (symlink-safe)")
    parser.add_argument(
        "--project-root",
        help="Path to the UE project root (auto-detected if omitted)")
    args = parser.parse_args()

    log_header("ReactorUMG Windows Setup (Python)")

    # Resolve paths
    paths = resolve_paths(args)
    plugin_dir = paths["plugin_dir"]
    project_root = paths["project_root"]

    log(f"Plugin dir:   {plugin_dir}")
    log(f"Project root: {project_root}")

    # Ensure toolchain
    log_header("Checking toolchain")
    ensure_tools()

    # Download V8
    log_header("V8 Engine Binaries")
    download_v8(plugin_dir)

    # Copy TypeScript template
    log_header("TypeScript Workspace")
    ts_dir = copy_ts_template(plugin_dir, project_root)

    # Copy system JS
    log_header("System JavaScript Files")
    copy_system_js(plugin_dir, project_root)

    # Build
    log_header("Building TypeScript Project")
    run_yarn_build(ts_dir)

    log_header("Setup Complete!")
    log("You can now open the project in Unreal Editor.")


if __name__ == "__main__":
    main()
