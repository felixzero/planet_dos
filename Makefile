WCL = wcl
WCC = wcc
DOSBOX = dosbox

DOSBOXCONF = dosbox.conf
OUTEXE = planet.exe
OBJFILES = main.o vga.o planet.o

${OUTEXE}: ${OBJFILES}
	${WCL} -bcl=dos -mc $^ -fe=$@

%.o: src/%.c
	${WCC} -bt=dos -mc -3 $^

project.dat:
	precomputation/project.py $@

clean:
	rm *.o

mrproper: clean
	rm ${OUTEXE}

run:
	${DOSBOX} -conf ${DOSBOXCONF} ${OUTEXE}

build_and_run: ${OUTEXE} run
