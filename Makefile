default:
	cd tools && ./build.sh

configure:
	cd tools && ./configure.sh

package:
	cd tools && ./package.sh

install:
	cd tools && ./install.sh

clean:
	cd tools && ./clean.sh

run:
	cd build/x64-linux/src/cli && ./mmcli

rungui:
	cd build/x64-linux/src/gui && ./mmgui
