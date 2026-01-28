#!/bin/bash

# OMNeT++ LEO Satellite Simulation - Build & Run Script

OMNET_HOME="$HOME/omnetpp-6.2.0"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== LEO Satellite Simulation ===${NC}"

# 1. Load OMNeT++ environment
echo -e "${YELLOW}Loading OMNeT++ environment...${NC}"
if [ -f "$OMNET_HOME/setenv" ]; then
    source "$OMNET_HOME/setenv"
else
    echo -e "${RED}ERROR: OMNeT++ not found at $OMNET_HOME${NC}"
    exit 1
fi

cd "$PROJECT_DIR"

# 2. Build
echo -e "${YELLOW}Building project...${NC}"
make MODE=release -j$(sysctl -n hw.ncpu)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"

# 3. Run simulation
CONFIG="${1:-TurkeyCoverage}"  # Default config, or pass as argument

echo -e "${YELLOW}Running simulation with config: ${CONFIG}${NC}"
cd simulations

../my-leo -m -u Qtenv -c "$CONFIG" -n "../src:." --image-path="../images" omnetpp.ini

echo -e "${GREEN}Simulation complete!${NC}"
echo -e "Results saved in: ${PROJECT_DIR}/simulations/results/"
