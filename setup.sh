#!/usr/bin/env bash
set -euo pipefail

###############################################################################
# setup.sh — Install project dependencies
###############################################################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Decompress block fixtures if not already present
for gz in "$SCRIPT_DIR"/fixtures/*.dat.gz; do
  dat="${gz%.gz}"
  if [[ ! -f "$dat" ]]; then
    echo "Decompressing $(basename "$gz")..."
    gunzip -k "$gz"
  fi
done

# Compiling Parser
echo "Compiling main.cpp..."
g++ -std=c++17 -O3 "$SCRIPT_DIR/parser/main.cpp" -o "$SCRIPT_DIR/block_parser"

# Install web backend dependencies
echo "Installing web dependencies..."
cd "$SCRIPT_DIR/web" && npm install --silent

# Install web frontend dependencies and build
echo "Building web client..."
cd "$SCRIPT_DIR/web/client" && npm install --silent && npm run build

echo "Setup complete"
