execute_process(COMMAND /bin/bash -c "cd ${CMAKE_INSTALL_PREFIX} && rm -f initrd && find . -print0 | cpio --null --create --quiet --format=newc > /tmp/initrd && mv /tmp/initrd .")
