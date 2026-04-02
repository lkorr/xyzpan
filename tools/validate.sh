#!/bin/bash
# XYZPan Plugin Validation Script
# Runs pluginval at three strictness tiers to catch crashes, NaN output,
# threading bugs, parameter automation issues, and VST3 compliance problems.
#
# Usage:
#   bash tools/validate.sh [level]
#   level: 5 (quick), 8 (stress), 10 (max), or "all" (default)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGINVAL="$SCRIPT_DIR/pluginval.exe"
OUTPUT_DIR="$SCRIPT_DIR/pluginval_output"

# Default VST3 path (Release build)
VST3_PATH="${VST3_PATH:-$SCRIPT_DIR/../build/plugin/XYZPan_artefacts/Release/VST3/XYZPan.vst3}"

# VST3 validator (Steinberg SDK)
VALIDATOR_FLAG=""
if [ -f "$SCRIPT_DIR/vstvalidator.exe" ]; then
    VALIDATOR_FLAG="--vst3validator $SCRIPT_DIR/vstvalidator.exe"
    echo "[+] VST3 validator found — Steinberg compliance checks enabled"
else
    echo "[!] No vstvalidator.exe found — skipping Steinberg VST3 compliance checks"
    echo "    Build it from: https://github.com/steinbergmedia/vst3sdk"
fi

# Verify plugin exists
if [ ! -e "$VST3_PATH" ]; then
    echo "[ERROR] Plugin not found at: $VST3_PATH"
    echo "        Build first: cmake --build build --config Release --target XYZPan_VST3"
    exit 1
fi

# Verify pluginval exists
if [ ! -f "$PLUGINVAL" ]; then
    echo "[ERROR] pluginval.exe not found at: $PLUGINVAL"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

LEVEL="${1:-all}"
FAILED=0

run_tier() {
    local tier_name="$1"
    shift
    echo ""
    echo "================================================================"
    echo "  $tier_name"
    echo "================================================================"
    if "$PLUGINVAL" "$@"; then
        echo "[PASS] $tier_name"
    else
        echo "[FAIL] $tier_name"
        FAILED=1
    fi
}

# ---------- Tier 1: Baseline DAW Compatibility ----------
run_level_5() {
    run_tier "Tier 1: Baseline (strictness 5)" \
        --strictness-level 5 \
        --sample-rates 44100,48000,96000,192000 \
        --block-sizes 32,64,128,256,512,1024,2048 \
        --timeout-ms 60000 \
        --output-dir "$OUTPUT_DIR" \
        --verbose \
        $VALIDATOR_FLAG \
        "$VST3_PATH"
}

# ---------- Tier 2: Stress Testing ----------
run_level_8() {
    run_tier "Tier 2: Stress (strictness 8)" \
        --strictness-level 8 \
        --sample-rates 44100,48000,96000 \
        --block-sizes 64,128,512,1024 \
        --repeat 3 \
        --randomise \
        --timeout-ms 120000 \
        --output-dir "$OUTPUT_DIR" \
        --verbose \
        $VALIDATOR_FLAG \
        "$VST3_PATH"
}

# ---------- Tier 3: Maximum Fuzz ----------
run_level_10() {
    run_tier "Tier 3: Maximum (strictness 10)" \
        --strictness-level 10 \
        --sample-rates 44100,48000,96000 \
        --block-sizes 64,256,1024 \
        --repeat 5 \
        --randomise \
        --timeout-ms 300000 \
        --output-dir "$OUTPUT_DIR" \
        --verbose \
        $VALIDATOR_FLAG \
        "$VST3_PATH"
}

case "$LEVEL" in
    5)    run_level_5 ;;
    8)    run_level_8 ;;
    10)   run_level_10 ;;
    all)
        run_level_5
        run_level_8
        run_level_10
        ;;
    *)
        echo "Usage: bash tools/validate.sh [5|8|10|all]"
        exit 1
        ;;
esac

echo ""
echo "================================================================"
if [ $FAILED -eq 0 ]; then
    echo "  ALL TIERS PASSED"
else
    echo "  VALIDATION FAILED — check logs in $OUTPUT_DIR"
fi
echo "================================================================"

exit $FAILED
