#!/bin/bash

# Configuration
APP_DIR="$HOME/.mot"
SOURCE_BIN="bin/client_linux_amd64"
TARGET_BIN_NAME="client"
WRAPPER_NAME="mot"

echo "=== MoT Installer ==="

# 1. Check if binary exists
if [ ! -f "$SOURCE_BIN" ]; then
    echo "❌ Error: Binary '$SOURCE_BIN' not found."
    echo "   Please run: make clean && make static-client"
    exit 1
fi

# 2. Create Installation Directory (~/.mot)
echo "📂 Creating install directory at $APP_DIR..."
mkdir -p "$APP_DIR/bin"
mkdir -p "$APP_DIR/log"

# 3. Copy the binary
echo "🚀 Copying binary..."
cp "$SOURCE_BIN" "$APP_DIR/bin/$TARGET_BIN_NAME"
chmod +x "$APP_DIR/bin/$TARGET_BIN_NAME"

# 4. Create the Wrapper Script
# This script cd's into ~/.mot before running the app, ensuring logs/session stay there.
echo "📝 Creating wrapper script..."
cat <<EOF > "$APP_DIR/$WRAPPER_NAME"
#!/bin/bash
# Move to the app directory so logs and session files are saved there
cd "$APP_DIR"
# Run the client, passing any arguments (like IP/Domain)
./bin/$TARGET_BIN_NAME "\$@"
EOF

chmod +x "$APP_DIR/$WRAPPER_NAME"

# 5. Create System-Wide Symlink
echo "🔗 Linking command '$WRAPPER_NAME' to /usr/local/bin..."
echo "   (You may be asked for your password)"

if sudo ln -sf "$APP_DIR/$WRAPPER_NAME" "/usr/local/bin/$WRAPPER_NAME"; then
    echo "✅ Success! Installed."
else
    echo "❌ Failed to create symlink. Try running with sudo."
    exit 1
fi

echo ""
echo "🎉 Installation Complete!"
echo "   You can now type '$WRAPPER_NAME' from anywhere to start the client."
echo "   Data and logs are stored in: $APP_DIR"
echo " 	 You can delete this folder !"