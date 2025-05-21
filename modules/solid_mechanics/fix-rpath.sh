#!/bin/bash

# === Editable Variables ===
CONDA_ENV_LIB="$HOME/miniforge/envs/moose/lib"  # Path to your Conda environment's lib folder
SEARCH_DIR="."                                   # Directory to recursively search for binaries

# === Safety backup directory ===
BACKUP_DIR="./rpath_backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

# === Function to fix a single binary ===
fix_binary_rpaths() {
  local file="$1"
  echo "🔧 Checking $file"

  # Backup original file
  cp "$file" "$BACKUP_DIR/$(basename "$file")"

  # Show current rpaths
  current_rpaths=$(otool -l "$file" | grep -A2 LC_RPATH | grep path | awk '{print $2}')

  seen=()
  for path in $current_rpaths; do
    if [[ " ${seen[@]} " =~ " ${path} " ]]; then
      echo "  ➖ Removing duplicate rpath: $path"
      install_name_tool -delete_rpath "$path" "$file"
    else
      seen+=("$path")
    fi
  done
}

# === Step 1: Recursively fix binaries in SEARCH_DIR ===
echo "🔍 Recursively fixing binaries in $SEARCH_DIR"
find "$SEARCH_DIR" -type f \( -perm +111 \) | while read file; do
  if file "$file" | grep -q "Mach-O"; then
    fix_binary_rpaths "$file"
  fi
done

# === Step 2: Fix Conda .dylib files ===
echo "🔍 Fixing Conda .dylib files in $CONDA_ENV_LIB"
find "$CONDA_ENV_LIB" -name "*.dylib" | while read dylib; do
  echo "🔧 Fixing $dylib"
  install_name_tool -delete_rpath "@loader_path" "$dylib" 2>/dev/null
  install_name_tool -add_rpath "@loader_path" "$dylib" 2>/dev/null
done

echo "✅ All done. Backup saved at $BACKUP_DIR"

