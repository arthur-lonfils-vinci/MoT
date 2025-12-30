#!/usr/bin/env bash
set -Eeuo pipefail

# =========================
# MoT Installer (updated)
# - Installs to: ~/.mot
# - Provides command: mot
# - Makes mot work in NEW terminals by adding ~/.mot to PATH (or creates symlink if possible)
# =========================

APP_DIR="${HOME}/.mot"
SOURCE_BIN="bin/client_linux_amd64"
TARGET_BIN_NAME="client"
WRAPPER_NAME="mot"

BIN_DIR="${APP_DIR}/bin"
LOG_DIR="${APP_DIR}/log"
WRAPPER_PATH="${APP_DIR}/${WRAPPER_NAME}"

GREEN="\033[0;32m"
YELLOW="\033[0;33m"
RED="\033[0;31m"
NC="\033[0m"

say()  { echo -e "${GREEN}$*${NC}"; }
info() { echo -e "${YELLOW}$*${NC}"; }
err()  { echo -e "${RED}$*${NC}" >&2; }

# -------- Helpers --------
command_exists() { command -v "$1" >/dev/null 2>&1; }

append_line_if_missing() {
  local file="$1"
  local line="$2"
  mkdir -p "$(dirname "$file")"
  touch "$file"
  if ! grep -Fqx "$line" "$file"; then
    echo "" >> "$file"
    echo "$line" >> "$file"
    return 0
  fi
  return 1
}

install_path_for_bash() {
  local rc="$HOME/.bashrc"
  local line='export PATH="$HOME/.mot:$PATH"'
  if append_line_if_missing "$rc" "# MoT CLI"; then :; fi
  if append_line_if_missing "$rc" "$line"; then
    say "➕ Added MoT to PATH in: $rc"
  else
    info "ℹ️  MoT PATH already present in: $rc"
  fi
}

install_path_for_zsh() {
  local rc="$HOME/.zshrc"
  local line='export PATH="$HOME/.mot:$PATH"'
  if append_line_if_missing "$rc" "# MoT CLI"; then :; fi
  if append_line_if_missing "$rc" "$line"; then
    say "➕ Added MoT to PATH in: $rc"
  else
    info "ℹ️  MoT PATH already present in: $rc"
  fi
}

install_path_for_fish() {
  # Fish universal vars is best, but we’ll modify config.fish for transparency.
  local rc="$HOME/.config/fish/config.fish"
  local line='set -gx PATH $HOME/.mot $PATH'
  if append_line_if_missing "$rc" "# MoT CLI"; then :; fi
  if append_line_if_missing "$rc" "$line"; then
    say "➕ Added MoT to PATH in: $rc"
  else
    info "ℹ️  MoT PATH already present in: $rc"
  fi
}

detect_shell_and_install_path() {
  # Install for the current user shell (and bash by default as a safe fallback)
  local shell_name
  shell_name="$(basename "${SHELL:-bash}")"

  case "$shell_name" in
    zsh)  install_path_for_zsh ;;
    fish) install_path_for_fish ;;
    *)    install_path_for_bash ;; # default
  esac

  # Also update bashrc if user is not bash but bash exists (common on Ubuntu)
  if [[ "$shell_name" != "bash" ]]; then
    install_path_for_bash
  fi
}

try_create_symlink() {
  # Optional: make it work system-wide if /usr/local/bin is available and sudo works.
  # PATH method already solves the “new terminal” problem without sudo.
  local link="/usr/local/bin/${WRAPPER_NAME}"

  if [[ -w "/usr/local/bin" ]]; then
    ln -sf "$WRAPPER_PATH" "$link"
    say "🔗 Symlink created: $link -> $WRAPPER_PATH (no sudo needed)"
    return 0
  fi

  if command_exists sudo; then
    info "🔗 Attempting to create system symlink in /usr/local/bin (may ask for password)..."
    if sudo -n true 2>/dev/null || sudo true; then
      sudo ln -sf "$WRAPPER_PATH" "$link"
      say "✅ Symlink created: $link -> $WRAPPER_PATH"
      return 0
    fi
  fi

  info "ℹ️  Skipping system symlink (not writable / sudo unavailable). PATH method will be used."
  return 0
}

print_post_install() {
  echo ""
  say "🎉 Installation Complete!"
  echo "Command:"
  echo "  ${WRAPPER_NAME}"
  echo ""
  echo "Installed in:"
  echo "  ${APP_DIR}"
  echo ""
  echo "Logs / data stored in:"
  echo "  ${LOG_DIR}"
  echo ""
  info "To use immediately in THIS terminal, run:"
  echo "  source ~/.bashrc  # or restart your terminal"
  echo ""
  info "Uninstall:"
  echo "  rm -rf \"${APP_DIR}\""
  echo "  # (and remove the MoT CLI lines from your shell rc file if you want)"
  echo ""
}

# =========================
# Main
# =========================
echo "=== MoT Installer ==="

# 1) Check binary exists
if [[ ! -f "$SOURCE_BIN" ]]; then
  err "❌ Error: Binary '$SOURCE_BIN' not found."
  err "   Please run: make clean && make static-client"
  exit 1
fi

# 2) Create install dirs
info "📂 Creating install directory at ${APP_DIR}..."
mkdir -p "$BIN_DIR" "$LOG_DIR"

# 3) Copy binary
info "🚀 Copying binary..."
cp -f "$SOURCE_BIN" "${BIN_DIR}/${TARGET_BIN_NAME}"
chmod +x "${BIN_DIR}/${TARGET_BIN_NAME}"

# 4) Create wrapper script
info "📝 Creating wrapper script..."
cat > "$WRAPPER_PATH" <<EOF
#!/usr/bin/env bash
set -Eeuo pipefail
APP_DIR="\$HOME/.mot"
cd "\$APP_DIR"
exec "\$APP_DIR/bin/${TARGET_BIN_NAME}" "\$@"
EOF
chmod +x "$WRAPPER_PATH"

# 5) Ensure it works in NEW terminals: add ~/.mot to PATH
info "🧭 Ensuring 'mot' works in new terminals (PATH update)..."
detect_shell_and_install_path

# 6) Optional system-wide symlink (nice-to-have)
try_create_symlink

# 7) Print next steps
print_post_install
