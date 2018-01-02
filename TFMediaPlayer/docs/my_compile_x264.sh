
XARCH="-arch aarch64"
export CC="xcrun -sdk iphoneos clang"

export AS="gas-preprocessor.pl -arch aarch64 -- xcrun -sdk iphoneos clang"


CONFIGURE_FLAGS="--enable-static --enable-pic --disable-cli"
HOST="--host=aarch64-apple-darwin"
CFLAGS="-arch arm64 -fembed-bitcode -mios-version-min=8.0"
ASFLAGS=$CFLAGS;
LDFLAGS=$CFLAGS;

/Users/apple/AVLibraries/x264/configure \
		    $CONFIGURE_FLAGS \
		    $HOST \
		    --extra-cflags="$CFLAGS" \
		    --extra-asflags="$ASFLAGS" \
		    --extra-ldflags="$LDFLAGS" \
		    --prefix="/Users/apple/AVLibraries/my_x264/arm64" || exit 1