#!/bin/bash

# Ensure a version argument is provided
if [ -z "$1" ]; then
    echo "❌ Error: No version specified."
    echo "Usage: ./setup_release.sh <version>"
    echo "Example: ./setup_release.sh v0.6"
    exit 1
fi

VERSION="$1"
RELEASE_NAME="mot-client-$VERSION"

echo "📦 Packaging Release: $RELEASE_NAME"

# 1. Rebuild to be sure
echo "🔨 Building static client..."
make clean && make static-client

# 2. Create a temporary folder for the release
echo "📂 Creating dist structure..."
# Remove old directory if it exists to ensure a clean state
rm -rf "dist/$RELEASE_NAME"
mkdir -p "dist/$RELEASE_NAME/bin"

# 3. Copy the Binary, Installer, and Uninstaller
echo "📋 Copying files..."
cp bin/client_linux_amd64 "dist/$RELEASE_NAME/bin/"
cp install.sh "dist/$RELEASE_NAME/"
cp uninstall.sh "dist/$RELEASE_NAME/"

# 4. Create the archive
echo "🗜️ Creating tarball..."
cd dist
tar -czvf "$RELEASE_NAME.tar.gz" "$RELEASE_NAME/"

# 5. Check the result
echo "✅ Release ready:"
ls -lh "$RELEASE_NAME.tar.gz"