#!/bin/bash
set -eou pipefail

evdi_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

if [ ! -v VIRTUAL_ENV ]; then
    if [ ! -d "${evdi_dir}"/.venv_evdi ]; then
        # prepare python virtualenv for tests
        python3 -m virtualenv "${evdi_dir}"/.venv_evdi
    fi
    # shellcheck disable=SC1091
    source "${evdi_dir}"/.venv_evdi/bin/activate
    pip3 install pytest
    pip3 install pytest-mock
    pip3 install pybind11
fi


# build module, library, pybindings
cd "${evdi_dir}"/library
make
cd "${evdi_dir}"/module
lsmod | grep evdi > /dev/null || sudo make install_dkms

cd "${evdi_dir}"/pyevdi
make

cp "${evdi_dir}"/library/libevdi.so* "${VIRTUAL_ENV}"/lib/
cp PyEvdi*.so* "$(pybind11-config --pkgconfigdir)"/../../../

# add evdi device
lsmod | grep evdi > /dev/null || sudo modprobe evdi
[ "$(cat /sys/devices/evdi/count)" == "0" ] && echo 1 | sudo tee -a /sys/devices/evdi/add && sleep 1

LD_LIBRARY_PATH=$VIRTUAL_ENV/lib pytest "$@"

