# 1. Rebuild to be sure
make clean && make static-client

# 2. Create a temporary folder for the release
mkdir -p dist/mot-client-v0.5/bin

# 3. Copy the Binary and the Installer
cp bin/client_linux_amd64 dist/mot-client-v0.5/bin/
cp install.sh dist/mot-client-v0.5/

# 4. Create the archive
cd dist
tar -czvf mot-client-v0.5.tar.gz mot-client-v0.5/

# 5. Check the result
ls -lh mot-client-v0.5.tar.gz