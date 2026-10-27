savedcmd_temp_P3.mod := printf '%s\n'   temp_P3.o | awk '!x[$$0]++ { print("./"$$0) }' > temp_P3.mod
