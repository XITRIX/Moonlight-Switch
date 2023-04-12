FROM archlinux:latest

ENV DEVKITPRO=/opt/devkitpro

RUN pacman-key --recv BC26F752D25B92CE272E0F44F7FD5492264BB9D0 --keyserver keyserver.ubuntu.com && \
    pacman-key --init && \
    pacman-key --lsign BC26F752D25B92CE272E0F44F7FD5492264BB9D0 && \
    pacman --noconfirm -Suy wget

RUN wget https://pkg.devkitpro.org/devkitpro-keyring.pkg.tar.xz && \
    pacman --noconfirm -U devkitpro-keyring.pkg.tar.xz && \
    rm devkitpro-keyring.pkg.tar.xz

RUN echo $'[dkp-libs] \n\
Server = https://pkg.devkitpro.org/packages \n\
\n\
[dkp-linux] \n\
Server = https://pkg.devkitpro.org/packages/linux/$arch/ \n\
' >> /etc/pacman.conf

WORKDIR /code

RUN pacman --noconfirm -Suy  &&  \
    pacman --noconfirm -Suy switch-dev &&\
    pacman -S --noconfirm make  \
    switch-ffmpeg  \
    switch-mbedtls  \
    switch-opusfile  \
    switch-sdl2  \
    switch-curl  \
    switch-libexpat  \
    switch-jansson  \
    switch-glfw  \
    switch-glm  \
    switch-libvpx  \
    switch-glad

CMD make clean; make -j8
