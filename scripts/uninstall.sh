#!/usr/bin/env bash
set -Eeuo pipefail

# =========================
# MoT Uninstaller
# =========================

APP_DIR="${HOME}/.mot"
WRAPPER_NAME="mot"
SYMLINK="/usr/local/bin/${WRAPPER_NAME}"

GREEN="\033[0;32m"
YELLOW="\033[0;33m"
RED="\033[0;31m"
NC="\033[0m"

say()  { echo -e "${GREEN}$*${NC}"; }
info() { echo -e "${YELLOW}$*${NC}"; }
err()  { echo -e "${RED}$*${NC}" >&2; }

# -------- Helpers --------
remove_lines_matching() {
  local file="$1"
  local pattern="$2"

  [[ -f "$file" ]] || return 0

  if grep -qE "$pattern" "$file"; then
    info "🧹 Cleaning PATH entries in $file"
    # Create backup once
    cp "$file" "${file}.mot.bak" 2>/dev/null || true
    # Remove matching lines
    sed -i "/$pattern/d" "$file"
  fi
}

# =========================
# Main
# =========================
echo "=== MoT Uninstaller ==="

# 1) Remove app directory
if [[ -d "$APP_DIR" ]]; then
  info "🗑️  Removing $APP_DIR"
  rm -rf "$APP_DIR"
  say "✅ Removed $APP_DIR"
else
  info "ℹ️  $APP_DIR not found"
fi

# 2) Remove system symlink if it points to MoT
if [[ -L "$SYMLINK" ]]; then
  TARGET="$(readlink "$SYMLINK" || true)"
  if [[ "$TARGET" == *".mot/mot" ]]; then
    info "🔗 Removing symlink $SYMLINK"
    if [[ -w "$(dirname "$SYMLINK")" ]]; then
      rm -f "$SYMLINK"
    elif command -v sudo >/dev/null 2>&1; then
      sudo rm -f "$SYMLINK"
    fi
    say "✅ Removed system symlink"
  else
    info "ℹ️  $SYMLINK exists but does not point to MoT — skipping"
  fi
else
  info "ℹ️  No system symlink found"
fi

# 3) Clean PATH entries (bash / zsh / fish)
remove_lines_matching "$HOME/.bashrc"    'MoT CLI|\.mot'
remove_lines_matching "$HOME/.zshrc"     'MoT CLI|\.mot'
remove_lines_matching "$HOME/.profile"   'MoT CLI|\.mot'
remove_lines_matching "$HOME/.config/fish/config.fish" 'MoT CLI|\.mot'

# 4) Final message
echo ""
say "🎉 MoT has been fully uninstalled."
info "You may want to restart your terminal or run:"
echo "  source ~/.bashrc"
echo ""
info "Backups of modified config files (if any):"
echo "  *.mot.bak"
echo ""
