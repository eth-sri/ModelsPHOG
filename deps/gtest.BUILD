cc_library(
    name = "gtest",
    srcs = glob(
        [
            "google*/src/*.cc",
        ],
        exclude = glob([
            "google*/src/*-all.cc",
            "googlemock/src/gmock_main.cc",
        ]),
    ),
    hdrs = glob(["*/include/**/*.h"]),
    includes = [
        "googlemock/",
        "googlemock/include",
        "googletest/",
        "googletest/include",
    ],
    linkopts = ["-pthread"],
    textual_hdrs = ["googletest/src/gtest-internal-inl.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "gtest_main",
    srcs = [
        "googletest/src/gtest_main.cc",
    ],
    deps = [ ":gtest" ],
    visibility = ["//visibility:public"],
)

