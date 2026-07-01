// elf_demo.c -- no usa libc para poder emitir ELF relocatable desde macOS.
extern int external_scale(int value);

int initialized = 7;
int zeroed;
const char label[] = "ELF";

int answer(void) {
    return initialized + zeroed + label[0];
}

int call_external(void) {
    return external_scale(answer());
}
