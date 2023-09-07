all: test

run: test
	adb push test /data/local/tmp
	adb shell /data/local/tmp/test	

test: test.c
	aarch64-linux-gnu-gcc test.c -static -o test

clean:
	rm -f test
