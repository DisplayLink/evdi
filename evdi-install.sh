#!/bin/bash
# remove default evdi module and build and install this fork
# clone this  https://github.com/pioto1225/evdi/tree/amd_vmap_texture
# put this script in the parent directory of evdi
echo "**** this script must be run from the parent of the the evdi repo ****"
ls -la /usr/src | grep evdi
echo enter the latest evdi driver version
read version
echo "***** moving /usr/src/evdi-$version to evdi-$version.original ****"
sudo mv /usr/src/evdi-$version evdi-$version.original
sudo chmod -R 0444 evdi-$version.original
echo "***** copying sudo evdi/module to /usr/src/evdi-$version ****"
sudo cp -R evdi/module /usr/src/evdi-$version
echo "**** uninstalling module evdi version $version ****"
sudo dkms uninstall evdi/$version
echo "**** unbuilding module evdi version $version ****"
sudo dkms unbuild evdi/$version
echo "**** building module evdi version $version *****"
sudo dkms build evdi/$version
echo "**** installing module evdi version $version ****"
sudo dkms install evdi/$version --force

LINE="vmap_texture=1"
FILE="/etc/modprobe.d/evdi.conf"
[[ $(cat $FILE | grep $LINE) ]] && exit 0
sudo sed -i "s/$/$LINE/" "$FILE"
echo "**** appended $LINE to $FILE ****"
cat $FILE
echo "**************"
dkms status
