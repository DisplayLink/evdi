#!/bin/bash
set -eou pipefail

if [ ! -d .venv_evdi ]; then
    # prepare python virtualenv for tests
    python3 -m virtualenv .venv_evdi
    source .venv_evdi/bin/activate
    pip3 install pytest
    pip3 install pytest-mock
    pip3 install pybind11

    # build module, library, pybindings
    cd ../library
    make
    cd ../module
    lsmod | grep evdi > /dev/null || sudo make install_dkms

    cd ../pyevdi
    make

    cp ../library/libevdi.so* .venv_evdi/lib/
    cp PyEvdi*.so* $(pybind11-config --pkgconfigdir)/../../../
    deactivate

    # add evdi device
    lsmod | grep evdi > /dev/null || sudo modprobe evdi
    [ $(cat /sys/devices/evdi/count) == "0" ] && echo 1 | sudo tee -a /sys/devices/evdi/add && sleep 1
fi

source .venv_evdi/bin/activate
LD_LIBRARY_PATH=$VIRTUAL_ENV/lib pytest $@

