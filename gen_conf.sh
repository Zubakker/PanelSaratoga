xdpyinfo | awk '/dimensions/{print $2}' | sed "s/^/int const SCREEN_W = /;s/x/\;\nint const SCREEN_H = /;s/$/\;/"  > config.h

