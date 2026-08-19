#!/bin/bash
# setup.sh - Pulls and builds dependencies locally
echo "Cloning libtailscale to build local binary..."
rm -rf /tmp/libtailscale
git clone https://github.com/tailscale/libtailscale.git /tmp/libtailscale
cd /tmp/libtailscale

sed -i 's/^go 1\..*/go 1.19/' go.mod

make libtailscale.a
cp libtailscale.a ~/TChat/
cd ~/TChat
rm -rf /tmp/libtailscale
echo "Done! You can now run 'make all'."
