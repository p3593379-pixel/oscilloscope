#!/usr/bin/env bash
# Generates a self-signed TLS cert for local development.
# Run once; commit the .crt but NOT the .key.
set -euo pipefail

OUT_DIR="/etc/nginx/ssl"
sudo mkdir -p "$OUT_DIR"

sudo openssl req -x509 -nodes -days 3650 \
    -newkey rsa:4096 \
    -keyout "$OUT_DIR/oscilloscope-selfsigned.key" \
    -out    "$OUT_DIR/oscilloscope-selfsigned.crt" \
    -subj "/CN=oscilloscope.local/O=Lab/C=NL" \
    -addext "subjectAltName=DNS:localhost,DNS:oscilloscope.local,IP:127.0.0.2"

echo "Certificate written to $OUT_DIR"
echo "Add 127.0.0.2 oscilloscope.local to /etc/hosts to use the friendly hostname."
