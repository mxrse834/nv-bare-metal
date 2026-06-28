savedcmd_temp_c.mod := printf '%s\n'   temp_c.o | awk '!x[$$0]++ { print("./"$$0) }' > temp_c.mod
