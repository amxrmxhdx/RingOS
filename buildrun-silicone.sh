arch -x86_64 make clean

# Use a larger default Cocoa window without forcing full-screen.
# You can override, e.g.:
# QEMU_UI_FLAGS="-display cocoa,zoom-to-fit=on" ./buildrun-silicone.sh
QEMU_UI_FLAGS=${QEMU_UI_FLAGS:-"-display cocoa,zoom-to-fit=on"}

# Fullscreen flags crash on this QEMU/Cocoa build. Resize window instead.
AUTO_RESIZE=${AUTO_RESIZE:-1}
QEMU_WINDOW_X=${QEMU_WINDOW_X:-20}
QEMU_WINDOW_Y=${QEMU_WINDOW_Y:-40}
QEMU_WINDOW_W=${QEMU_WINDOW_W:-1400}
QEMU_WINDOW_H=${QEMU_WINDOW_H:-900}

arch -x86_64 make run QEMU_VGA=std QEMU_FLAGS="-no-reboot ${QEMU_UI_FLAGS}" &
RUN_PID=$!

if [ "$AUTO_RESIZE" = "1" ]; then
	for _ in $(seq 1 20); do
		osascript >/dev/null 2>&1 <<EOF && break
tell application "System Events"
	tell process "QEMU"
		if (count of windows) > 0 then
			set position of front window to {${QEMU_WINDOW_X}, ${QEMU_WINDOW_Y}}
			set size of front window to {${QEMU_WINDOW_W}, ${QEMU_WINDOW_H}}
			return
		end if
	end tell
end tell
EOF
		sleep 0.2
	done
fi

wait $RUN_PID