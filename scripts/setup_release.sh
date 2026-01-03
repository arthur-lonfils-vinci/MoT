#!/bin/bash

# Exit on error
set -e

# 1. Load Admin Configuration (.env)
# This allows you to keep your production secrets out of the repo
if [ -f ".env" ]; then
    echo "Loading secrets from .env..."
    export $(grep -v '^#' .env | xargs)
else
    echo "/!\ .env not found! Using defaults (127.0.0.1:8010)."
    OFFICIAL_HOST="127.0.0.1"
    OFFICIAL_PORT="8010"
fi

# Ensure a version argument is provided
if [ -z "$1" ]; then
    echo "X Error: No version specified."
    echo "Usage: ./scripts/setup_release.sh <version>"
    echo "Example: ./scripts/setup_release.sh v0.6.1-alpha"
    exit 1
fi

VERSION="$1"
RELEASE_NAME="mot-client-$VERSION"
DIST_DIR="dist/$RELEASE_NAME"
ARCHIVE="$RELEASE_NAME.tar.gz"

echo "1) Starting Release Build $VERSION..."
echo "   Server Target: $OFFICIAL_HOST:$OFFICIAL_PORT"

# 2. Clean & Build Static Binary
echo "2) Compiling static binary..."
make clean
# Ensure the certificate is embedded before building
./scripts/embed_cert.sh
make static-client

# 3. Prepare Distribution Folder
echo "3) Preparing package structure..."
rm -rf "dist"
mkdir -p "$DIST_DIR/bin"

# Copy Binary
# We rename it to 'client' for simplicity in the package
cp bin/client_linux_amd64 "$DIST_DIR/bin/client"

# Copy Scripts & Docs
cp scripts/install.sh "$DIST_DIR/"
cp scripts/uninstall.sh "$DIST_DIR/"
cp doc/README_CLIENT.md "$DIST_DIR/"

# 4. BAKE CONFIGURATION (The Injection Step)
# We use sed to replace the placeholders in the COPIED install.sh
# This ensures the user gets an installer pre-configured for YOUR server
echo "4)  Injecting server configuration..."
sed -i "s|__OFFICIAL_HOST__|$OFFICIAL_HOST|g" "$DIST_DIR/install.sh"
sed -i "s|__OFFICIAL_PORT__|$OFFICIAL_PORT|g" "$DIST_DIR/install.sh"

# Make scripts executable
chmod +x "$DIST_DIR/install.sh"
chmod +x "$DIST_DIR/uninstall.sh"
chmod +x "$DIST_DIR/bin/client"

# 5. Create Archive
echo "🗜️  Compressing..."
cd dist
tar -czf "$ARCHIVE" "$RELEASE_NAME"

echo ""
echo "5) Release Ready: dist/$ARCHIVE"
echo "   (Upload this file to GitHub Releases)"