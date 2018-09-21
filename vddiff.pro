DEFINES = \
    DEBUG \
    HAVE_FUTIMENS BIN='""' \
    HAVE_LIBAVLBST \
    HAVE_MKDTEMP \
    HAVE_NCURSESW_CURSES_H \
    LEX_HAS_BUFS \
    TEST \
    TEST_DIR \
    TRACE='""' \
    USE_SYS_SYSMACROS_H \
    VDSUSP

SOURCES += \
    fs_test.cpp \
    test.cpp \
    cplt.c \
    db.c \
    diff.c \
    dl.c \
    ed.c \
    exec.c \
    fs.c \
    gq.c \
    info.c \
    main.c \
    misc.c \
    tc.c \
    ui.c \
    ui2.c \
    uzp.c \
    misc_test.cpp

HEADERS += \
    cplt.h \
    db.h \
    diff.h \
    dl.h \
    ed.h \
    exec.h \
    fs_test.h \
    fs.h \
    gq.h \
    info.h \
    lex.h \
    lex.l \
    main.h \
    misc.h \
    pars.h \
    pars.y \
    tc.h \
    test.h \
    ui.h \
    ui2.h \
    uzp.h \
    misc_test.h \
    ver.h
