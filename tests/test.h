#ifndef EMS_TEST_H
#define EMS_TEST_H

#define TEST_ASSERT(e)                                                         \
    do {                                                                       \
    if (!(e)) {                                                                \
        warnx("Assertion failed: %s (%s:%d)\n", #e, __FILE__, (int)__LINE__);  \
	abort();                                                               \
    }                                                                          \
    } while (0)

#define TEST_selected(f) \
    (test_onecasename == NULL || !strcmp(test_onecasename, #f)) 

#define TEST(f) if (TEST_selected(f)) test(#f, f, 0, 0);
#define TEST_sig(f, s) if (TEST_selected(f)) test(#f, f, 0, s);
#define TEST_exit(f, e) if (TEST_selected(f)) test(#f, f, e, 0);

char *test_onecasename;

void test_init(int, char**, void (*)(void), void (*)(void));
void test(char *, void (*)(void), int, int);
void test_done(void);

void *emalloc(size_t);
char *ecreatetmpf(int);
void eremove(char*);

#endif /* EMS_TEST_H */
