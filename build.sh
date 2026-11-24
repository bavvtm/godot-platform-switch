#!/bin/bash

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE}")" && pwd)"

source "${ROOT_DIR}/config"

BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
GODOT_OVERLAY_DIR="${GODOT_OVERLAY_DIR:-${ROOT_DIR}/platform-overlay}"
GODOT_SOURCE="${GODOT_SOURCE:-${BUILD_DIR}/godot}"
MESA_OVERLAY_DIR="${MESA_OVERLAY_DIR:-${ROOT_DIR}/mesa}"
MESA_SOURCE="${MESA_SOURCE:-${BUILD_DIR}/mesa}"
MESA_BUILD_TYPE="${MESA_BUILD_TYPE:-debug}"
EXPORTER_SRC_DIR="${EXPORTER_SRC_DIR:-${ROOT_DIR}/exporter}"
GODOT_CPP_SOURCE="${GODOT_CPP_SOURCE:-${BUILD_DIR}/godot-cpp}"
GODOT_CPP_TARGET="${GODOT_CPP_TARGET:-template_debug}"
ADDON_OUT_DIR="${ADDON_OUT_DIR:-${ROOT_DIR}/addons/switch_exporter}"
JOBS=${JOBS:-$(nproc)}

function clone_mesa() {
  if [[ ! -d "${MESA_SOURCE}/.git" ]]; then
    git init "${MESA_SOURCE}" || return 1
    git -C "${MESA_SOURCE}" remote add origin "${MESA_REPO}" || return 1
    git -C "${MESA_SOURCE}" fetch --depth 1 origin "${MESA_REF}" || return 1
    git -C "${MESA_SOURCE}" checkout FETCH_HEAD || return 1
  fi

  if [[ -d "${MESA_OVERLAY_DIR}" ]]; then
    cp -f "${MESA_OVERLAY_DIR}/"* "${MESA_SOURCE}/"
    chmod +x "${MESA_SOURCE}/"*.sh 2>/dev/null || true
  fi

  return 0
}

function build_mesa() {
  if [[ -d "${MESA_SOURCE}/mesa-unified-install" ]]; then
    return 0
  fi

  (
    cd "${MESA_SOURCE}" || exit 1
    MESA_BUILD_TYPE="${MESA_BUILD_TYPE}" ./build-unified-linux.sh
  ) || return 1

  return 0
}

function clone_godot() {
  if [[ ! -d "${GODOT_SOURCE}/.git" ]]; then
    git clone --depth 1 --branch "${GODOT_REF}" "${GODOT_REPO}" "${GODOT_SOURCE}" || return 1
  fi

  rm -rf "${GODOT_SOURCE}/platform/switch"
  ln -sfn "${GODOT_OVERLAY_DIR}" "${GODOT_SOURCE}/platform/switch"

  return 0
}

function build_godot() {
	local target=$1
  local obj_dir="${GODOT_SOURCE}/build/obj_${target}"
  local mesa_sdk="${MESA_SOURCE}/mesa-unified-install/opt/devkitpro/portlibs/switch"
  local debug_symbols="yes"

  if [[ "${target}" == "release" ]]; then
    debug_symbols="no"
  fi

  python3 -m SCons -C "${GODOT_SOURCE}" \
    platform=switch \
    target="template_${target}" \
    arch=arm64 \
    vulkan=yes \
    opengl3=yes \
    audren=yes \
    use_lto=no \
    debug_symbols="${debug_symbols}" \
    mesa_sdk="${mesa_sdk}" \
    object_dir="${obj_dir}" \
    -j"${JOBS}" || return 1

  aarch64-none-elf-strip --strip-all \
    "${GODOT_SOURCE}/bin/godot.switch.template_${target}.arm64" || return 1

	cp -f "${GODOT_SOURCE}/bin/godot.switch.template_${target}.arm64" \
    "${BUILD_DIR}/${target}"

  cp -f "${GODOT_SOURCE}/bin/godot.switch.template_${target}.arm64.nro" \
    "${BUILD_DIR}/Godot_${GODOT_REF}_switch.${target}.nro"

  return 0
}

function clone_godot_cpp() {
  if [[ ! -d "${GODOT_CPP_SOURCE}/.git" ]]; then
    git clone --depth 1 --branch "${GODOT_CPP_REF}" \
      "${GODOT_CPP_REPO}" \
      "${GODOT_CPP_SOURCE}" || return 1
  fi

  return 0
}

function build_exporter() {
  mkdir -p "${ADDON_OUT_DIR}"

  python3 -m SCons -C "${EXPORTER_SRC_DIR}" \
    platform=linux \
    target="${GODOT_CPP_TARGET}" \
    godot_cpp_dir="${GODOT_CPP_SOURCE}" \
    addon_dir="${ADDON_OUT_DIR}" \
    -j"${JOBS}" || return 1

  if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    python3 -m SCons -C "${EXPORTER_SRC_DIR}" \
      platform=windows \
      target="${GODOT_CPP_TARGET}" \
      use_mingw=yes \
      use_static_cpp=yes \
      godot_cpp_dir="${GODOT_CPP_SOURCE}" \
      addon_dir="${ADDON_OUT_DIR}" \
      -j"${JOBS}" || return 1
  else
    echo "Info: mingw-w64 not found, skipping Windows .dll build."
  fi

  return 0
}

function make_zip() {
  local zip_file_name="switch_exporter-godot_${GODOT_REF}.zip"
  local zip_output="${BUILD_DIR}/${zip_file_name}"

  rm -f "${ADDON_OUT_DIR}/.gdignore"

  cp -f "${GODOT_OVERLAY_DIR}/romfs/applet_splash.rgba.z" \
    "${ADDON_OUT_DIR}/romfs/applet_splash.rgba.z"

  if [ -f "${BUILD_DIR}/debug" ]; then
    mv -f "${BUILD_DIR}/debug" "${ADDON_OUT_DIR}/templates/debug"
  fi

  if [ -f "${BUILD_DIR}/release" ]; then
    mv -f "${BUILD_DIR}/release" "${ADDON_OUT_DIR}/templates/release"
  fi

  if [ ! -f "${ADDON_OUT_DIR}/libswitch_exporter.linux.template_release.x86_64.so" ] && \
     [ ! -f "${ADDON_OUT_DIR}/libswitch_exporter.windows.template_release.x86_64.dll" ]; then
    echo "Error: Exporter shared libraries (.so / .dll) not found in ${ADDON_OUT_DIR}!"
    return 1
  fi

  (
    cd "${ROOT_DIR}" && \
    zip -r -q "${zip_output}" "addons/switch_exporter" \
      -x "*.gitkeep" "*.tmp*" "*/.gdignore"
  )

  return 0
}

TARGET_MODE="${2:-both}"
if [[ "${TARGET_MODE}" == "debug" ]]; then
  GODOT_TARGETS=("debug")
elif [[ "${TARGET_MODE}" == "release" ]]; then
  GODOT_TARGETS=("release")
else
  GODOT_TARGETS=("debug" "release")
fi

mkdir -p "${BUILD_DIR}"

case "${1:-all}" in
  format)
    find "${GODOT_OVERLAY_DIR}" "${EXPORTER_SRC_DIR}/src" -type f \( -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} +
    ;;
  mesa)
    clone_mesa || exit 1
    build_mesa || exit 1
    ;;
  godot)
    clone_godot || exit 1
    for t in "${GODOT_TARGETS[@]}"; do
      build_godot "$t" || exit 1
    done
    ;;
  exporter)
    clone_godot_cpp || exit 1
    build_exporter || exit 1
    ;;
  all|*)
    clone_mesa || exit 1
    clone_godot || exit 1
    clone_godot_cpp || exit 1
    build_mesa || exit 1
    for t in "${GODOT_TARGETS[@]}"; do
      build_godot "$t" || exit 1
    done
    build_exporter || exit 1
    make_zip || exit 1
    ;;
esac
