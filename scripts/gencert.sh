#!/bin/bash
echo "🔐 Generating Self-Signed Certificate..."

# Generate private key
openssl genrsa -out server.key 2048

# Generate certificate (valid for 365 days)
# -nodes: No password on key
# -subj: Avoid interactive prompts
openssl req -new -x509 -key server.key -out server.crt -days 365 -nodes \
    -subj "/C=US/ST=State/L=City/O=MoT/OU=Server/CN=localhost"

echo "✅ Generated server.key and server.crt"