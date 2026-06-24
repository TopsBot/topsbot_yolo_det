#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/aliyun_download.sh
source "${SCRIPT_DIR}/lib/aliyun_download.sh"
# shellcheck source=lib/path_utils.sh
source "${SCRIPT_DIR}/lib/path_utils.sh"

MANIFEST="${SCRIPT_DIR}/../config/models_manifest.yaml"
INSTALL_DIR=""
MODELS_ZIP_URL=""
DEBUG="false"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Download models.zip from Aliyun and extract .nb files into install_dir.

Options:
  --manifest PATH   Manifest yaml (default: package config/models_manifest.yaml)
  --install-dir DIR Override install_dir from manifest
  --url URL         Override models_zip_url from manifest
  --debug           Enable Aliyun API debug logs
  -h, --help        Show this help
EOF
}

read_manifest_field() {
    local manifest="$1"
    local field="$2"
    python3 - "$manifest" "$field" <<'PY'
import sys

try:
    import yaml
except ImportError:
    print("ERROR: python3-yaml is required to read models_manifest.yaml", file=sys.stderr)
    sys.exit(2)

manifest_path, field = sys.argv[1], sys.argv[2]
with open(manifest_path, "r", encoding="utf-8") as handle:
    data = yaml.safe_load(handle) or {}

value = data.get(field)
if value is None:
    sys.exit(1)
if isinstance(value, list):
    for item in value:
        print(item)
else:
    print(value)
PY
}

while [ $# -gt 0 ]; do
    case "$1" in
        --manifest)
            MANIFEST="$2"
            shift 2
            ;;
        --install-dir)
            INSTALL_DIR="$2"
            shift 2
            ;;
        --url)
            MODELS_ZIP_URL="$2"
            shift 2
            ;;
        --debug)
            DEBUG="true"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [ "$DEBUG" = "true" ]; then
    ALIYUN_DEBUG="true"
fi

if [ ! -f "$MANIFEST" ]; then
    log_error "Manifest not found: $MANIFEST"
    exit 1
fi

if [ -z "$INSTALL_DIR" ]; then
    INSTALL_DIR="${TOPSBOT_MODEL_DIR:-$(read_manifest_field "$MANIFEST" install_dir)}"
fi
INSTALL_DIR="$(expand_user_path "$INSTALL_DIR")"

if [ -z "$MODELS_ZIP_URL" ]; then
    MODELS_ZIP_URL=$(read_manifest_field "$MANIFEST" models_zip_url)
fi

if [[ "$MODELS_ZIP_URL" == *"PLACEHOLDER"* ]]; then
    log_error "models_zip_url still contains PLACEHOLDER; edit config/models_manifest.yaml first"
    exit 1
fi

check_download_deps

log_info "Install dir: $INSTALL_DIR"
log_info "Downloading models.zip ..."

MODEL_FILE_ID=$(get_auto_file_id "$MODELS_ZIP_URL" "models")
ZIP_PATH="$(mktemp /tmp/topsbot_models.XXXXXX.zip)"
trap 'rm -f "$ZIP_PATH"' EXIT

download_aliyun_file "$MODELS_ZIP_URL" "$MODEL_FILE_ID" "$ZIP_PATH"

mkdir -p "$INSTALL_DIR"
# models.zip 根目录即为 .nb 文件，丢弃路径直接解压
unzip -o -j "$ZIP_PATH" -d "$INSTALL_DIR"

mapfile -t REQUIRED_MODELS < <(read_manifest_field "$MANIFEST" required_models)
missing=0
for model in "${REQUIRED_MODELS[@]}"; do
    if [ ! -f "${INSTALL_DIR}/${model}" ]; then
        log_error "Missing after extract: ${INSTALL_DIR}/${model}"
        missing=1
    else
        log_success "Ready: ${INSTALL_DIR}/${model}"
    fi
done

if [ "$missing" -ne 0 ]; then
    log_error "models.zip content does not match required_models in manifest"
    exit 1
fi

log_success "All models installed under $INSTALL_DIR"
