#!/bin/bash

#args: directory to save built

if [[ "$#" -ne "1" ]]; then
  echo "USAGE:"
  echo ".sh %directory_to_save_built%"
  exit 2
fi

final_dir="$1"
init_dir="$(pwd)"

build_dir="cpp/build"

rm -rf "${build_dir}"
mkdir "${build_dir}"
test -d "${build_dir}"
if [[ $(echo $?) -eq 1 ]]; then
  echo "CANNOT CREATE BUILD DIR"
  exit 1
fi

cd "${build_dir}" || exit 1
cmake ..
cmake --build ./

cd "${init_dir}" || exit 1
cp "${build_dir}/"*.a "${final_dir}"

rm -rf "${build_dir}"

exit 0
