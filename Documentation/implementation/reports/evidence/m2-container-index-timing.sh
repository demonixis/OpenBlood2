set -eu
for cfg in debug release; do
 for run in 1 2 3; do
  for version in before after; do
   /tmp/b2-container-index-$cfg-$version '/home/yann/.steam/steam/steamapps/common/Blood II/' 01_SubwayA > /tmp/b2-container-index-$cfg-timing-$run-$version.log 2>&1
  done
 done
done
