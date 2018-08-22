#ifndef TEST_H_
#define TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FATAL_ERROR \
    throw std::runtime_error{ \
        __FILE__ + \
        std::string{" "} + \
        std::to_string(__LINE__)}

extern bool printerr_called;

void test(void);

#ifdef __cplusplus
}
#endif

#endif
