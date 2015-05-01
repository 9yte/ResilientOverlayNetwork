source info.sh
if [ "a$1" != "a" ]; then node="client-$1"; fi
./ron.out --ip $ip --port $port --map $map --node $node --user $user --pass "$pass" --id $user
