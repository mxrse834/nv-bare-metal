savedcmd_temp_P2.mod := printf '%s\n'   temp_P2.o | awk '!x[$$0]++ { print("./"$$0) }' > temp_P2.mod
