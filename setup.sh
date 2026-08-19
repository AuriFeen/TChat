#!/bin/bash
# setup.sh - Pulls and builds dependencies locally
echo "Cloning libtailscale to build local binary..."
git clone https://github.com/tailscale/libtailscale.git /tmp/libtailscale
cd /tmp/libtailscale
make libtailscale.a
cp libtailscale.a ~/TChat/
cd ~/TChat
rm -rf /tmp/libtailscale
echo "Done! You can now run 'make all'."
