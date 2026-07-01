extern int external_data;
extern int external_call(int value);

int use_external(void) {
    return external_data + external_call(7);
}
