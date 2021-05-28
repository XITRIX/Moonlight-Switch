mkfile_path    :=    $(abspath $(lastword $(MAKEFILE_LIST)))

SOURCES		:=	$(SOURCES) \
				app/src/switch \
				app/src/crypto \
				app/src/streaming \
				app/src/streaming/audio \
				app/src/streaming/ffmpeg \
				app/src/streaming/switch \
				app/src/streaming/video \
				app/src/streaming/video/Deko3D \
				app/src/libgamestream \
				app/src/utils 

INCLUDES	:=	$(INCLUDES) \
				app/include \
				app/src/crypto \
				app/src/streaming \
				app/src/streaming/audio \
				app/src/streaming/ffmpeg \
				app/src/streaming/switch \
				app/src/streaming/video \
				app/src/streaming/video/Deko3D \
				app/src/libgamestream \
				app/src/utils \
				app/src/switch 